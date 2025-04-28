#pragma once

#include "tinygltf/tiny_gltf.h"
#define GLM_FORCE_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <filesystem>
#include <iostream>

namespace Renderer{
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
static inline glm::vec4 to_vec4(R8G8B8A8_U const& color){
    return glm::vec4(static_cast<float>(color.r), static_cast<float>(color.g), static_cast<float>(color.b), static_cast<float>(color.a)) / 255.f;
}

enum class CullMode{
    NONE, CLOCK_WISE, COUNTER_CLOCK_WISE,
};

struct Vertex{
    glm::vec4 position;
    glm::vec2 texcoord0;
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

enum class DepthTestMode{
    NEVER, ALWAYS, LESS, LESSEQUAL, GREATER, GREATEREQUAL, EQUAL, NOTEQUAL,
};

struct DepthSettings{
    bool write = true;
    DepthTestMode test_mode = DepthTestMode::ALWAYS;
};

bool depth_test_passed(DepthTestMode mode, std::uint32_t value, std::uint32_t reference) {
    switch (mode) {
    case DepthTestMode::NEVER:
        return false;
    case DepthTestMode::ALWAYS:
        return true;
    case DepthTestMode::LESS:
        return value < reference;
    case DepthTestMode::LESSEQUAL:
        return value <= reference;
    case DepthTestMode::GREATER:
        return value > reference;
    case DepthTestMode::GREATEREQUAL:
        return value >= reference;
    case DepthTestMode::EQUAL:
        return value == reference;
    case DepthTestMode::NOTEQUAL:
        return value != reference;
    default:
        return false;
    }
}

template<typename PixelType>
struct Image{
    std::vector<PixelType> image;
    std::uint32_t width, height;
    PixelType& at(std::uint32_t x, std::uint32_t y) {
        return image[y * width + x];
    }
};

template<typename PixelType>
struct ImageView{
    PixelType* image = nullptr;
    std::uint32_t width, height;
    PixelType& at(std::uint32_t x, std::uint32_t y) {
        return image[y * width + x];
    }
};

template<typename PixelType>
ImageView<PixelType> create_imageview(const Image<PixelType>& image, const uint32_t width, const uint32_t height){
    return ImageView{
        .image = (PixelType*)image.image.data(),
        .width = width,
        .height = height,
    };
}

template<typename PixelType>
struct Texture {
    std::vector<Image<PixelType>> mipmaps;
};

struct DrawCall {
    CullMode cull_mode = CullMode::NONE;
    DepthSettings depth_settings = {};
    std::vector<Vertex>* vertex_buffer = nullptr;
    std::vector<std::uint32_t>* index_buffer = nullptr;
    Texture<R8G8B8A8_U>* texture = nullptr;
    glm::mat4 transform = glm::identity<glm::mat4>();
};

struct FrameBuffer{
    ImageView<R8G8B8A8_U> color_buffer_view;
    ImageView<std::uint32_t> depth_buffer_view;
};

template<typename PixelType>
void clear(ImageView<PixelType>& image_view, PixelType color) {
    std::fill_n(image_view.image, image_view.width * image_view.height, color);
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
    v.texcoord0 = (1.f - t) * v0.texcoord0 + t * v1.texcoord0;

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

void draw(FrameBuffer* frame_buffer, DrawCall const& command, ViewPort const& viewport) {
    for (std::uint32_t idx_idx = 0; idx_idx + 2 < command.index_buffer->size(); idx_idx+= 3){
        std::uint32_t i0 = command.index_buffer->at(idx_idx + 0);
        std::uint32_t i1 = command.index_buffer->at(idx_idx + 1);
        std::uint32_t i2 = command.index_buffer->at(idx_idx + 2);

        Vertex vertices[12];
        vertices[0] = command.vertex_buffer->at(i0);
        vertices[1] = command.vertex_buffer->at(i1);
        vertices[2] = command.vertex_buffer->at(i2);

        vertices[0].position = command.transform * vertices[0].position;
        vertices[1].position = command.transform * vertices[1].position;
        vertices[2].position = command.transform * vertices[2].position;
        
        // this clipping algorithm is taken from https://lisyarus.github.io/blog/posts/implementing-a-tiny-cpu-rasterizer-part-5.html#section-clipping-triangles-implementation
        auto end = clip_triangle(vertices, vertices + 3);

        for (auto triangle_begin = vertices; triangle_begin < end; triangle_begin += 3){
            Vertex v0 = triangle_begin[0];
            Vertex v1 = triangle_begin[1];
            Vertex v2 = triangle_begin[2];
            
            v0.position = perspective_divide(v0.position);
            v1.position = perspective_divide(v1.position);
            v2.position = perspective_divide(v2.position);

            v0.position = apply(viewport, v0.position);
            v1.position = apply(viewport, v1.position);
            v2.position = apply(viewport, v2.position);

            float det012 = det(v1.position.xy - v0.position.xy, v2.position.xy - v0.position.xy);

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
            std::int32_t xmax = std::min<std::int32_t>(viewport.x + viewport.width, frame_buffer->color_buffer_view.width)-1;
            std::int32_t ymin = std::max<std::int32_t>(viewport.y, 0);
            std::int32_t ymax = std::min<std::int32_t>(viewport.y + viewport.height, frame_buffer->color_buffer_view.height)-1;

            xmin = static_cast<int32_t>(std::max<float>(static_cast<float>(xmin), std::min({std::floor(v0.position.x), std::floor(v1.position.x), std::floor(v2.position.x)})));  
            xmax = static_cast<int32_t>(std::min<float>(static_cast<float>(xmax), std::max({ std::ceil(v0.position.x), std::ceil(v1.position.x), std::ceil(v2.position.x)})));
            ymin = static_cast<int32_t>(std::max<float>(static_cast<float>(ymin), std::min({ std::floor(v0.position.y), std::floor(v1.position.y), std::floor(v2.position.y)})));
            ymax = static_cast<int32_t>(std::min<float>(static_cast<float>(ymax), std::max({ std::ceil(v0.position.y), std::ceil(v1.position.y), std::ceil(v2.position.y)})));

            for (std::int32_t y = ymin; y < ymax; y+=2){
                for (std::int32_t x = xmin; x < xmax; x+=2){

                    using array2x2 = std::array<std::array<float, 2>, 2>;
                    array2x2 det01p;
                    array2x2 det12p;
                    array2x2 det20p;

                    array2x2 l0;
                    array2x2 l1;
                    array2x2 l2;

                    std::array<std::array<glm::vec2, 2>, 2> tex_coord;

                    for (int dy = 0; dy < 2; dy++){
                        for (int dx = 0; dx < 2; dx++){
                            glm::vec4 p = glm::vec4(
                                x+dx+0.5f, y+dy+0.5f, 0.f, 0.f
                            );

                            det01p[dy][dx] = det(v1.position - v0.position, p - v0.position);
                            det12p[dy][dx] = det(v2.position - v1.position, p - v1.position);
                            det20p[dy][dx] = det(v0.position - v2.position, p - v2.position);

                            l0[dy][dx] = det12p[dy][dx] / det012 * v0.position.w;
                            l1[dy][dx] = det20p[dy][dx] / det012 * v1.position.w;
                            l2[dy][dx] = det01p[dy][dx] / det012 * v2.position.w;

                            float lsum = l0[dy][dx] + l1[dy][dx] + l2[dy][dx];

                            l0[dy][dx] /= lsum;
                            l1[dy][dx] /= lsum;
                            l2[dy][dx] /= lsum;

                            tex_coord[dy][dx] = l0[dy][dx] * v0.texcoord0 + l1[dy][dx] * v1.texcoord0 + l2[dy][dx] * v2.texcoord0;
                        }
                    }

                    for (int dy = 0; dy < 2; dy++) {
                        for (int dx = 0; dx < 2; dx++) {
                            if (x + dx > xmax || y + dy > ymax) continue;
                            if (det01p[dy][dx] < 0.f || det12p[dy][dx] < 0.f || det20p[dy][dx] < 0.f) continue;

                            auto ndc_position = l0[dy][dx] * v0.position + l1[dy][dx] * v1.position + l2[dy][dx] * v2.position;

                            std::uint32_t depth = static_cast<uint32_t>((0.5f + 0.5f * ndc_position.z) * UINT32_MAX);

                            if (!depth_test_passed(command.depth_settings.test_mode, depth, frame_buffer->depth_buffer_view.at(x + dx, y + dy)))
                                continue;

                            if (command.depth_settings.write)
                                frame_buffer->depth_buffer_view.at(x + dx, y + dy) = depth;

                            glm::vec4 color = l0[dy][dx] * v0.position + l1[dy][dx] * v1.position + l2[dy][dx] * v2.position;

                            auto albedo = command.texture;
                            glm::vec2 texture_scale(albedo->mipmaps[0].width, albedo->mipmaps[0].height);
                            glm::vec2 tc = texture_scale * tex_coord[dy][dx];
                            glm::vec2 tc_dx = texture_scale * (tex_coord[dy][1] - tex_coord[dy][0]);
                            glm::vec2 tc_dy = texture_scale * (tex_coord[1][dx] - tex_coord[0][dx]);

                            float texel_area = 1.f / std::abs(det(tc_dx, tc_dy));
                            bool magnification = texel_area >= 1.f;

                            Image<R8G8B8A8_U>* mipmap;
                            
                            int mipmap_level = 0;
                            if (magnification) {
                                mipmap = &albedo->mipmaps[0];
                            }
                            else {
                                mipmap_level = static_cast<int>(std::ceil(-std::log2(std::min(1.f, texel_area)) / 2.f));
                                mipmap = &albedo->mipmaps[std::min<int>(mipmap_level, static_cast<int>(albedo->mipmaps.size()) - 1)];
                            }

                            // tc.x = mipmap->width * std::fmod(tex_coord[dy][dx].x, 1.f);
                            // tc.y = mipmap->height * std::fmod(tex_coord[dy][dx].y, 1.f);

                            // if (mipmap->width == 1 || mipmap->height == 1){
                            //     int ix = static_cast<int>(std::floor(tc.x));
                            //     int iy = static_cast<int>(std::floor(tc.y));

                            //     auto texel = mipmap->at(ix, iy);
                            //     color = to_vec4(texel);
                            // }
                            // else {
                            //     tc.x -= 0.5f;
                            //     tc.y -= 0.5f;

                            //     tc.x = std::max<float>(0.f, std::min<float>(mipmap->width - 1.f, tc.x));
                            //     tc.y = std::max<float>(0.f, std::min<float>(mipmap->height - 1.f, tc.y));

                            //     int ix = std::min<int>(mipmap->width - 2, static_cast<int>(std::floor(tc.x)));
                            //     int iy = std::min<int>(mipmap->height - 2, static_cast<int>(std::floor(tc.y)));

                            //     tc.x -= ix;
                            //     tc.y -= iy;

                            //     std::array<glm::vec4, 4>samples = {
                            //         to_vec4(mipmap->at(ix + 0, iy + 0)),
                            //         to_vec4(mipmap->at(ix + 1, iy + 0)),
                            //         to_vec4(mipmap->at(ix + 0, iy + 1)),
                            //         to_vec4(mipmap->at(ix + 1, iy + 1)),
                            //     };

                            //     color = (1.f - tc.y) * ((1.f - tc.x) * samples[0] + tc.x * samples[1]) + tc.y * ((1.f - tc.x) * samples[2] + tc.x * samples[3]);
                            // }

                            frame_buffer->color_buffer_view.at(x + dx, y + dy) = to_r8g8b8a8_u(color);
                            // frame_buffer->color_buffer_view.at(x + dx, y + dy) = to_r8g8b8a8_u(glm::vec4(glm::vec3(static_cast<float>(mipmap_level) / albedo->mipmaps.size()), 1.f));
                        }
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

Renderer::Image<Renderer::R8G8B8A8_U> load_image(std::filesystem::path const& path) {
    uint32_t width, height;
    int channels;
    char path_char[1024];
    size_t size;
    wcstombs_s(&size, path_char, path.c_str(), path.string().size());
    // wcstombs(path_char, path.c_str(), path.string().size());
    Renderer::R8G8B8A8_U* data = (Renderer::R8G8B8A8_U*) stbi_load(path_char, (int*)&width, (int*)&height, &channels, 4);
    std::cout << "load file:" << path << ", size=" << width << "x" << height << std::endl;
    Renderer::Image<Renderer::R8G8B8A8_U> result {
        .image = std::vector<Renderer::R8G8B8A8_U>(data, data + width * height),
        .width = width, 
        .height = height,
    };
    return result;
}

void generate_mipmaps(Renderer::Texture<Renderer::R8G8B8A8_U>* texture){
    if (texture->mipmaps.empty()) return;

    texture->mipmaps.resize(1);

    for (int i = 1;;i++){
        auto& prev_level = texture->mipmaps[i - 1];

        if (prev_level.width == 1 && prev_level.height == 1)
            break;

        std::uint32_t new_width = prev_level.width / 2 + (prev_level.width & 1);
        std::uint32_t new_height = prev_level.height / 2 + (prev_level.height & 1);

        Renderer::Image<Renderer::R8G8B8A8_U> next_level = {
            .image = std::vector<Renderer::R8G8B8A8_U>(new_width * new_height),
            .width = new_width,
            .height = new_height,
        };

        auto get_pixel = [&](std::uint32_t x, std::uint32_t y){
            return to_vec4(prev_level.at(std::min(x, prev_level.width - 1), std::min(y, prev_level.height - 1)));
        };

        for (std::uint32_t y = 0; y < new_height; y++){
            for (std::uint32_t x = 0; x < new_width; x++){
                glm::vec4 result(0.f, 0.f, 0.f, 0.f);

                result += get_pixel(2 * x + 0, 2 * y + 0);
                result += get_pixel(2 * x + 1, 2 * y + 0);
                result += get_pixel(2 * x + 0, 2 * y + 1);
                result += get_pixel(2 * x + 1, 2 * y + 1);

                result /= 4.f;

                next_level.at(x, y) = Renderer::to_r8g8b8a8_u(result);
            }
        }

        texture->mipmaps.push_back(std::move(next_level));
    }
}
       
}