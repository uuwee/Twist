#include "tinygltf/tiny_gltf.h"
#define GLM_FORCE_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <chrono>
#include <algorithm>
#include <execution>
#include <cstdint>
#include <memory>
#include <initializer_list>

struct R8G8B8A8_U{
    std::uint8_t r, g, b, a;
};
static inline std::uint32_t ToUint32(R8G8B8A8_U color) {
    return color.r << 24 | color.g << 16 | color.b << 8 | color.a << 0;
}
static inline R8G8B8A8_U to_r8g8b8a8_u(glm::vec4 const & color) {
    return {
        .r = (uint8_t)std::max(0.f, std::min(255.f, color.x * 255.f)),
        .g = (uint8_t)std::max(0.f, std::min(255.f, color.y * 255.f)),
        .b = (uint8_t)std::max(0.f, std::min(255.f, color.z * 255.f)),
        .a = (uint8_t)std::max(0.f, std::min(255.f, color.w * 255.f)),
    };
}

enum class CullMode{
    NONE, CLOCK_WISE, COUNTER_CLOCK_WISE,
};

struct Vertex{
    glm::vec4 position;
    // glm::vec3 normal;
    // glm::vec2 texcoord0;
};

struct Mesh {
    std::vector<glm::vec3> vertex;
    // std::vector<glm::vec3> normal;
    // std::vector<glm::vec2> texcoord0;
    std::vector<std::uint32_t> index;
};

struct ViewPort{
    std::uint32_t x, y, width, height;
};
glm::vec4 apply(ViewPort const& vp, glm::vec4 const& vertex) {
    const float fx = static_cast<float>(vp.x);
    const float fy = static_cast<float>(vp.y);
    const float fw = static_cast<float>(vp.width);
    const float fh = static_cast<float>(vp.height);

    const float ndcX = vertex.x;          // [-1, +1]
    const float ndcY = vertex.y;          // [-1, +1]

    const float winX = (ndcX + 1.0f) * 0.5f * fw + fx;
    const float winY = (1.0f - ndcY) * 0.5f * fh + fy;

    return{
        winX, winY, vertex.z, vertex.w
    };
}

struct DrawCall {
    CullMode cull_mode = CullMode::NONE;
    Mesh* mesh = nullptr;
    glm::mat4 transform = glm::identity<glm::mat4>();
};

template<std::uint32_t width, std::uint32_t height>
struct ImageView{
    std::array<R8G8B8A8_U, width * height>* image = nullptr;

    R8G8B8A8_U& at(std::uint32_t x, std::uint32_t y) {
        return image->data()[y * width + x];
    }
};

template<std::uint32_t width, std::uint32_t height>
void clear(ImageView<width, height>& image_view, R8G8B8A8_U color) {
    std::fill_n(image_view.image->data(), width * height, color);
}

float det(glm::vec2 const& a, glm::vec2 const& b) {
    return a.x * b.y - a.y * b.x;
}

Vertex clip_intersect_edge(Vertex const & v0, Vertex const & v1, float value0, float value1)
{
    // f(t) = at+b
    // f(0) = v0 = b
    // f(1) = v1 = a+v0 => a = v1 - v0
    // f(t) = v0 + (v1 - v0) * t
    // f(t) = 0 => t = -v0 / (v1 - v0) = v0 / (v0 - v1)

    float t = value0 / (value0 - value1);

    Vertex v;
    v.position = (1.f - t) * v0.position + t * v1.position;
    // v.normal = (1.f - t) * v0.normal + t * v1.normal;
    // v.texcoord0 = (1.f - t) * v0.texcoord0 + t * v1.texcoord0;

    return v;
}

Vertex * clip_triangle(Vertex * triangle, glm::vec4 equation, Vertex * result)
{
    float values[3] =
    {
        glm::dot(triangle[0].position, equation),
        glm::dot(triangle[1].position, equation),
        glm::dot(triangle[2].position, equation),
    };

    std::uint8_t mask = (values[0] < 0.f ? 1 : 0) | (values[1] < 0.f ? 2 : 0) | (values[2] < 0.f ? 4 : 0);

    switch (mask)
    {
    case 0b000:
        // All vertices are inside allowed half-space
        // No clipping required, copy the triangle to output
        *result++ = triangle[0];
        *result++ = triangle[1];
        *result++ = triangle[2];
        break;
    case 0b001:
        // Vertex 0 is outside allowed half-space
        // Replace it with points on edges 01 and 02
        // And re-triangulate
        {
            auto v01 = clip_intersect_edge(triangle[0], triangle[1], values[0], values[1]);
            auto v02 = clip_intersect_edge(triangle[0], triangle[2], values[0], values[2]);
            *result++ = v01;
            *result++ = triangle[1];
            *result++ = triangle[2];
            *result++ = v01;
            *result++ = triangle[2];
            *result++ = v02;
        }
        break;
    case 0b010:
        // Vertex 1 is outside allowed half-space
        // Replace it with points on edges 10 and 12
        // And re-triangulate
        {
            auto v10 = clip_intersect_edge(triangle[1], triangle[0], values[1], values[0]);
            auto v12 = clip_intersect_edge(triangle[1], triangle[2], values[1], values[2]);
            *result++ = triangle[0];
            *result++ = v10;
            *result++ = triangle[2];
            *result++ = triangle[2];
            *result++ = v10;
            *result++ = v12;
        }
        break;
    case 0b011:
        // Vertices 0 and 1 are outside allowed half-space
        // Replace them with points on edges 02 and 12
        *result++ = clip_intersect_edge(triangle[0], triangle[2], values[0], values[2]);
        *result++ = clip_intersect_edge(triangle[1], triangle[2], values[1], values[2]);
        *result++ = triangle[2];
        break;
    case 0b100:
        // Vertex 2 is outside allowed half-space
        // Replace it with points on edges 20 and 21
        // And re-triangulate
        {
            auto v20 = clip_intersect_edge(triangle[2], triangle[0], values[2], values[0]);
            auto v21 = clip_intersect_edge(triangle[2], triangle[1], values[2], values[1]);
            *result++ = triangle[0];
            *result++ = triangle[1];
            *result++ = v20;
            *result++ = v20;
            *result++ = triangle[1];
            *result++ = v21;
        }
        break;
    case 0b101:
        // Vertices 0 and 2 are outside allowed half-space
        // Replace them with points on edges 01 and 21
        *result++ = clip_intersect_edge(triangle[0], triangle[1], values[0], values[1]);
        *result++ = triangle[1];
        *result++ = clip_intersect_edge(triangle[2], triangle[1], values[2], values[1]);
        break;
    case 0b110:
        // Vertices 1 and 2 are outside allowed half-space
        // Replace them with points on edges 10 and 20
        *result++ = triangle[0];
        *result++ = clip_intersect_edge(triangle[1], triangle[0], values[1], values[0]);
        *result++ = clip_intersect_edge(triangle[2], triangle[0], values[2], values[0]);
        break;
    case 0b111:
        // All vertices are outside allowed half-space
        // Clip the whole triangle, result is empty
        break;
    }

    return result;
}

Vertex * clip_triangle(Vertex* begin, Vertex* end)
{
    static glm::vec4 const equations[2] =
    {
        {0.f, 0.f,  1.f, 1.f}, // Z > -W  =>   Z + W > 0
        {0.f, 0.f, -1.f, 1.f}, // Z <  W  => - Z + W > 0
    };

    Vertex result[12];

    for (auto equation : equations)
    {
        auto result_end = result;

        for (Vertex* triangle = begin; triangle != end; triangle += 3)
            result_end = clip_triangle(triangle, equation, result_end);

        end = std::copy(result, result_end, begin);
    }

    return end;
}

inline glm::vec4 perspective_divide(glm::vec4 const& v) {
    auto w = 1.f / v.w;
    return glm::vec4(v.x * w, v.y * w, v.z * w, w);
}

template<std::uint32_t width, std::uint32_t height>
void draw(ImageView<width, height>* color_buffer, DrawCall const& command, ViewPort const& viewport = {0, 0, width, height}) {
    for (std::uint32_t idx_idx = 0; idx_idx + 2 < command.mesh->index.size(); idx_idx+= 3){
        std::uint32_t i0 = command.mesh->index[idx_idx + 0];
        std::uint32_t i1 = command.mesh->index[idx_idx + 1];
        std::uint32_t i2 = command.mesh->index[idx_idx + 2];

        Vertex vertices[12];
        vertices[0] = Vertex{
            .position = command.transform * glm::vec4(command.mesh->vertex[i0], 1.0), 
            // .normal = command.mesh->normal[i0], 
            // .texcoord0 = command.mesh->texcoord0[i0]
        };
        vertices[1] = Vertex{
            .position = command.transform * glm::vec4(command.mesh->vertex[i1], 1.0), 
            // .normal = command.mesh->normal[i1], 
            // .texcoord0 = command.mesh->texcoord0[i1]
        };
        vertices[2] = Vertex{
            .position = command.transform * glm::vec4(command.mesh->vertex[i2], 1.0), 
            // .normal = command.mesh->normal[i2], 
            // .texcoord0 = command.mesh->texcoord0[i2]
        };

        // this clipping algorithm is taken from https://lisyarus.github.io/blog/posts/implementing-a-tiny-cpu-rasterizer-part-5.html#section-clipping-triangles-implementation
        auto end = clip_triangle(vertices, vertices + 3);

        for (auto triangle_begin = vertices; triangle_begin < end; triangle_begin += 3){
            glm::vec4 v0 = triangle_begin[0].position;
            glm::vec4 v1 = triangle_begin[1].position;
            glm::vec4 v2 = triangle_begin[2].position;
            
            v0 = perspective_divide(v0);
            v1 = perspective_divide(v1);
            v2 = perspective_divide(v2);

            v0 = apply(viewport, v0);
            v1 = apply(viewport, v1);
            v2 = apply(viewport, v2);

            float det012 = det(v1.xy - v0.xy, v2.xy - v0.xy);

            const bool is_ccw = det012 < 0.f;
            switch (command.cull_mode) 
            {
            case CullMode::NONE:
                if (is_ccw)
                {
                    std::swap(v1, v2);
                    det012 = -det012;
                }
                break;
            case CullMode::CLOCK_WISE:
                if (!is_ccw)
                    continue;
                std::swap(v1, v2);
                det012 = -det012;
                break;
            case CullMode::COUNTER_CLOCK_WISE:
                if (is_ccw)
                    continue;   
                break;
            default:
                break;
            }

            std::int32_t xmin = std::max<std::int32_t>(viewport.x, 0);
            std::int32_t xmax = std::min<std::int32_t>(viewport.x + viewport.width, width)-1;
            std::int32_t ymin = std::max<std::int32_t>(viewport.y, 0);
            std::int32_t ymax = std::min<std::int32_t>(viewport.y + viewport.height, height)-1;

            xmin = static_cast<int32_t>(std::max<float>(static_cast<float>(xmin), std::min({std::floor(v0.x), std::floor(v1.x), std::floor(v2.x)})));  
            xmax = static_cast<int32_t>(std::min<float>(static_cast<float>(xmax), std::max({ std::ceil(v0.x), std::ceil(v1.x), std::ceil(v2.x)})));
            ymin = static_cast<int32_t>(std::max<float>(static_cast<float>(ymin), std::min({ std::floor(v0.y), std::floor(v1.y), std::floor(v2.y)})));
            ymax = static_cast<int32_t>(std::min<float>(static_cast<float>(ymax), std::max({ std::ceil(v0.y), std::ceil(v1.y), std::ceil(v2.y)})));

            for (std::int32_t y = ymin; y < ymax; y++){
                for (std::int32_t x = xmin; x < xmax; x++){
                    glm::vec4 p = glm::vec4(
                        x+0.5f, y+ 0.5f, 0.f, 0.f
                    );

                    float det01p = det(v1.xy - v0.xy, p.xy - v0.xy);
                    float det12p = det(v2.xy - v1.xy, p.xy - v1.xy);
                    float det20p = det(v0.xy - v2.xy, p.xy - v2.xy);
                    

                    if (det01p >= 0.f && det12p >= 0.f && det20p >= 0.f) {
                        float l0 = det12p / det012 * v0.w;
                        float l1 = det20p / det012 * v1.w;
                        float l2 = det01p / det012 * v2.w;
                        float lsum = l0 + l1 + l2;
                        l0 /= lsum;
                        l1 /= lsum;
                        l2 /= lsum;
                        color_buffer->at(x, y) = to_r8g8b8a8_u(glm::vec4(l0, l1, l2, 1.f));
                    }
                }
            }
        }
    }
}

std::uint32_t bits_reverse( std::uint32_t v )
{
    v = (v & 0x55555555) <<  1 | (v >>  1 & 0x55555555);
    v = (v & 0x33333333) <<  2 | (v >>  2 & 0x33333333);
    v = (v & 0x0f0f0f0f) <<  4 | (v >>  4 & 0x0f0f0f0f);
    v = (v & 0x00ff00ff) <<  8 | (v >>  8 & 0x00ff00ff);
    v = (v & 0x0000ffff) << 16 | (v >> 16 & 0x0000ffff);
    return v;
}

void dump_surface_to_ppm(SDL_Surface const& surface){
    auto now = std::chrono::high_resolution_clock::now();
    std::ofstream out_File("./bin/output.ppm");
    if (!out_File) {
        std::cerr << "Error creating output file." << std::endl;
        return;
    }

    /*
    CAUTION:
    We input R8G8B8A8, but SDL_Surface.pixels is somehow ABGR8G8R8*.
    */

    out_File << "P3\n" << surface.w << " " << surface.h << "\n255\n";
    for (int y = 0; y < surface.h; ++y) {
        for (int x = 0; x < surface.w; ++x) {
            std::uint32_t pixel = ((std::uint32_t*)surface.pixels)[y * surface.w + x];
            uint8_t r = (pixel >> 0) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t b = (pixel >> 16) & 0xFF;
            uint8_t a = (pixel >> 24) & 0xFF;
            out_File << static_cast<int>(r) << " " << static_cast<int>(g) << " " << static_cast<int>(b) << "\n";
        }
    }
    out_File.close();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count();
    std::cout << "Saved image to output.ppm in " << duration << "ms" << std::endl;
}

int main() {
    std::cout << "hello, world!" << std::endl;
    constexpr int width = 600, height = 400;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Twist", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    SDL_Surface* draw_surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_SetSurfaceBlendMode(draw_surface, SDL_BLENDMODE_NONE);

    ImageView<width, height> render_target_view = {
        .image = reinterpret_cast<std::array<R8G8B8A8_U, width * height>*>(draw_surface->pixels),
    };

    Mesh mesh = {
        .vertex = {
            // -X face
            {-1.f, -1.f, -1.f},
            {-1.f,  1.f, -1.f},
            {-1.f, -1.f,  1.f},
            {-1.f,  1.f,  1.f},

            // +X face
            { 1.f, -1.f, -1.f},
            { 1.f,  1.f, -1.f},
            { 1.f, -1.f,  1.f},
            { 1.f,  1.f,  1.f},

            // -Y face
            {-1.f, -1.f, -1.f},
            { 1.f, -1.f, -1.f},
            {-1.f, -1.f,  1.f},
            { 1.f, -1.f,  1.f},

            // +Y face
            {-1.f,  1.f, -1.f},
            { 1.f,  1.f, -1.f},
            {-1.f,  1.f,  1.f},
            { 1.f,  1.f,  1.f},

            // -Z face
            {-1.f, -1.f, -1.f},
            { 1.f, -1.f, -1.f},
            {-1.f,  1.f, -1.f},
            { 1.f,  1.f, -1.f},

            // +Z face
            {-1.f, -1.f,  1.f},
            { 1.f, -1.f,  1.f},
            {-1.f,  1.f,  1.f},
            { 1.f,  1.f,  1.f},
        },
        .index = {
            // -X face
            0,  2,  1,
            1,  2,  3,

            // +X face
            4,  5,  6,
            6,  5,  7,

            // -Y face
            8,  9, 10,
            10,  9, 11,

            // +Y face
            12, 14, 13,
            14, 15, 13,

            // -Z face
            16, 18, 17,
            17, 18, 19,

            // +Z face
            20, 21, 22,
            21, 23, 22,
        },
    };
    
    R8G8B8A8_U clear_color = {255, 200, 200, 255};

    // timer
    auto last_frame_start = std::chrono::high_resolution_clock::now();

    // state
    bool running = true;
    bool dump_image = false;

    glm::vec3 camera_pos = {0.f, 0.f, -1.f};
    float y_rotation = 0.f;
    glm::vec2 mouse_pos = {0.f, 0.f};

    while(running) {
        for (SDL_Event event; SDL_PollEvent(&event); ) switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym)
            {
            case SDL_KeyCode::SDLK_ESCAPE:
                running = false;
                break;
            case SDL_KeyCode::SDLK_p:
                // dump_surface_to_ppm(*draw_surface);
                dump_image = true;
                break;
            case SDL_KeyCode::SDLK_w:
                camera_pos.y += 0.1f;
                break;
            case SDL_KeyCode::SDLK_s:
                camera_pos.y -= 0.1f;
                break;
            case SDL_KeyCode::SDLK_a:
                camera_pos.x -= 0.1f;
                break;
            case SDL_KeyCode::SDLK_d:
                camera_pos.x += 0.1f;
                break;
            case SDL_KeyCode::SDLK_q:
                camera_pos.z -= 0.1f;
                break;
            case SDL_KeyCode::SDLK_e:
                camera_pos.z += 0.1f;
                break;
            
            default:
                break;
            }
        case SDL_MOUSEMOTION:
            mouse_pos.x = static_cast<float>(event.motion.x);
            mouse_pos.y = static_cast<float>(event.motion.y);
            break;
        
        default:
            break;
        }

        if (!running) break;

        auto now = std::chrono::high_resolution_clock::now();
        auto delta_time = std::chrono::duration<float>(now - last_frame_start).count();
        last_frame_start = now;

        // std::cout << "delta_time: " << delta_time << std::endl;
        std::cout << "FPS: " << 1.0f / delta_time << std::endl;
        
        // clear color
        clear(render_target_view, clear_color);  

        y_rotation += delta_time;
        auto model_mat = glm::identity<glm::mat4>();
        model_mat = glm::translate(model_mat, glm::vec3(0.f, 0.f, -2.f));
        model_mat = glm::rotate(model_mat, y_rotation, glm::vec3(0.f, 1.f, 0.f));
        model_mat = glm::rotate(model_mat, y_rotation*0.1f, glm::vec3(1.f, 1.f, 0.f));
        auto view_mat = glm::identity<glm::mat4>();
        view_mat = glm::translate(view_mat, camera_pos);
        auto proj_mat = glm::perspective(glm::radians(90.0f), static_cast<float>(width) / height, 0.1f, 100.f);
        draw(&render_target_view, {
            .cull_mode = CullMode::NONE,
            .mesh = &mesh,
            .transform = proj_mat * view_mat * model_mat,
        });

        SDL_Rect rect{
            .x = 0, .y = 0, .w = width, .h = height
        };
        SDL_BlitSurface(draw_surface, &rect, SDL_GetWindowSurface(window), &rect);
        SDL_UpdateWindowSurface(window);

        if (dump_image) {
            dump_surface_to_ppm(*draw_surface);
            dump_image = false;
        }
    };

    return 0;
}