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

struct Mesh {
    std::vector<glm::vec3> vertex;
    std::vector<glm::vec3> normal;
    std::vector<glm::vec2> texcoord0;
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

template<std::uint32_t width, std::uint32_t height>
void draw(ImageView<width, height>* color_buffer, DrawCall const& command, ViewPort const& viewport = {0, 0, width, height}) {
    for (std::uint32_t idx_idx = 0; idx_idx + 2 < command.mesh->index.size(); idx_idx+= 3){
        glm::vec4 v0 = command.transform * glm::vec4(command.mesh->vertex[command.mesh->index[idx_idx + 0]], 1.f);
        glm::vec4 v1 = command.transform * glm::vec4(command.mesh->vertex[command.mesh->index[idx_idx + 1]], 1.f);
        glm::vec4 v2 = command.transform * glm::vec4(command.mesh->vertex[command.mesh->index[idx_idx + 2]], 1.f);
        
        v0 = apply(viewport, v0);
        v1 = apply(viewport, v1);
        v2 = apply(viewport, v2);

        std::uint32_t xmin = std::max<std::uint32_t>(viewport.x, 0);
        std::uint32_t xmax = std::min<std::uint32_t>(viewport.x + viewport.width, width)-1;
        std::uint32_t ymin = std::max<std::uint32_t>(viewport.y, 0);
        std::uint32_t ymax = std::min<std::uint32_t>(viewport.y + viewport.height, height)-1;

        xmin = std::max(xmin, std::min({ static_cast<std::uint32_t>(std::floor(v0.x)), static_cast<std::uint32_t>(std::floor(v1.x)), static_cast<std::uint32_t>(std::floor(v2.x))}));  
        xmax = std::min(xmax, std::max({ static_cast<std::uint32_t>(std::ceil(v0.x)), static_cast<std::uint32_t>(std::ceil(v1.x)), static_cast<std::uint32_t>(std::ceil(v2.x))}));
        ymin = std::max(ymin, std::min({ static_cast<std::uint32_t>(std::floor(v0.y)), static_cast<std::uint32_t>(std::floor(v1.y)), static_cast<std::uint32_t>(std::floor(v2.y))}));
        ymax = std::min(ymax, std::max({ static_cast<std::uint32_t>(std::ceil(v0.y)), static_cast<std::uint32_t>(std::ceil(v1.y)), static_cast<std::uint32_t>(std::ceil(v2.y))}));

        for (std::uint32_t y = ymin; y < ymax; y++){
            for (std::uint32_t x = xmin; x < xmax; x++){
                glm::vec4 p = {
                    x+0.5f, y+ 0.5f, 0.f, 1.f
                };

                float det01p = det(v1.xy - v0.xy, p.xy - v0.xy);
                float det12p = det(v2.xy - v1.xy, p.xy - v1.xy);
                float det20p = det(v0.xy - v2.xy, p.xy - v2.xy);
                float det012 = det(v1.xy - v0.xy, v2.xy - v0.xy);

                const bool is_ccw_tri = det012 < 0.f;
                
                if (det01p >= 0.f && det12p >= 0.f && det20p >= 0.f) {
                    float l0 = det12p / det012;
                    float l1 = det20p / det012;
                    float l2 = det01p / det012;

                    color_buffer->at(x, y) = to_r8g8b8a8_u(glm::vec4(l0, l1, l2, 1.f));
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
            { 0.f,   0.5f, 0.f},
            { 0.5f, -0.5f, 0.f},
            {-0.5f, -0.5f, 0.f},
        },
        .index = {
            0, 1, 2,
        },
    };
    glm::mat4 model_matrix = glm::identity<glm::mat4>();
    glm::mat4 view_matrix = glm::identity<glm::mat4>();
    glm::mat4 projection_matrix = glm::perspective(glm::radians(45.f), static_cast<float>(width) / height, 0.1f, 100.f);
    projection_matrix[1][1] *= -1.f; // flip y axis
    
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

        draw(&render_target_view, {
            .mesh = &mesh,
            .transform = glm::identity<glm::mat4>(),
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