#pragma once

#include "tinygltf/tiny_gltf.h"
#define GLM_FORCE_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#define SDL_MAIN_HANDLED
#include <SDL.h>

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

enum class CullMode{
    NONE, CLOCK_WISE, COUNTER_CLOCK_WISE,
};

struct Vertex{
    glm::vec4 position;
    // glm::vec3 normal;
    glm::vec2 texcoord0;
};

struct Mesh {
    std::vector<glm::vec3> vertex;
    // std::vector<glm::vec3> normal;
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

template<std::uint32_t width, std::uint32_t height, typename PixelType>
struct Image{
    std::array<PixelType, width * height> image;
};

template<typename PixelType>
struct ImageView{
    PixelType* image = nullptr;
    std::uint32_t width, height;
    PixelType& at(std::uint32_t x, std::uint32_t y) {
        return image[y * width + x];
    }
};

template<std::uint32_t width, std::uint32_t height, typename PixelType>
ImageView<PixelType> create_imageview(const Image<width, height, PixelType>& image){
    return ImageView{
        .image = (PixelType*)image.image.data(),
        .width = width,
        .height = height,
    };
}

struct Sampler{
    enum class Filtering{
        NEAREST,
        LINEAR,
    };
    Filtering mag_filter;
    Filtering min_filter;
};

struct DrawCall {
    CullMode cull_mode = CullMode::NONE;
    DepthSettings depth_settings = {};
    Mesh* mesh = nullptr;
    Sampler* sampler = nullptr;
    ImageView<R8G8B8A8_U>* texture = nullptr;
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
    for (std::uint32_t idx_idx = 0; idx_idx + 2 < command.mesh->index.size(); idx_idx+= 3){
        std::uint32_t i0 = command.mesh->index[idx_idx + 0];
        std::uint32_t i1 = command.mesh->index[idx_idx + 1];
        std::uint32_t i2 = command.mesh->index[idx_idx + 2];

        Vertex vertices[12];
        vertices[0] = Vertex{
            .position = command.transform * glm::vec4(command.mesh->vertex[i0], 1.0), 
            // .normal = command.mesh->normal[i0], 
            .texcoord0 = command.mesh->texcoord0[i0]
        };
        vertices[1] = Vertex{
            .position = command.transform * glm::vec4(command.mesh->vertex[i1], 1.0), 
            // .normal = command.mesh->normal[i1], 
            .texcoord0 = command.mesh->texcoord0[i1]
        };
        vertices[2] = Vertex{
            .position = command.transform * glm::vec4(command.mesh->vertex[i2], 1.0), 
            // .normal = command.mesh->normal[i2], 
            .texcoord0 = command.mesh->texcoord0[i2]
        };

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

            for (std::int32_t y = ymin; y < ymax; y++){
                for (std::int32_t x = xmin; x < xmax; x++){
                    glm::vec4 p = glm::vec4(
                        x+0.5f, y+ 0.5f, 0.f, 0.f
                    );

                    float det01p = det(v1.position.xy - v0.position.xy, p.xy - v0.position.xy);
                    float det12p = det(v2.position.xy - v1.position.xy, p.xy - v1.position.xy);
                    float det20p = det(v0.position.xy - v2.position.xy, p.xy - v2.position.xy);
                    

                    if (det01p >= 0.f && det12p >= 0.f && det20p >= 0.f) {
                        float l0 = det12p / det012 * v0.position.w;
                        float l1 = det20p / det012 * v1.position.w;
                        float l2 = det01p / det012 * v2.position.w;
                        float lsum = l0 + l1 + l2;
                        l0 /= lsum;
                        l1 /= lsum;
                        l2 /= lsum;

                        {
                            float z = l0 * v0.position.z + l1 * v1.position.z + l2 * v2.position.z;
                            std::uint32_t depth = static_cast<std::uint32_t>((0.5f + 0.5f * z) * UINT32_MAX);

                            auto& old_depth = frame_buffer->depth_buffer_view.at(x, y);
                            if (!depth_test_passed(command.depth_settings.test_mode, depth, old_depth)) {
                                continue;
                            }
                            if (command.depth_settings.write) {
                                frame_buffer->depth_buffer_view.at(x, y) = depth;
                            }
                        }

                        glm::vec2 texcoord = l0 * v0.texcoord0 + l1 * v1.texcoord0 + l2 * v2.texcoord0;
                        glm::vec4 color = glm::vec4(texcoord.x , texcoord.y, 0.f, 1.f);
                        if (int(std::floor(color.x * 8) + std::floor(color.y * 8)) % 2 == 0)
                            frame_buffer->color_buffer_view.at(x, y) = {0, 0, 0, 255};
                        else
                            frame_buffer->color_buffer_view.at(x, y) = {255, 255, 255, 255};
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
}