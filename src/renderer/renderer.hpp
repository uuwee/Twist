#pragma once

#include "tinygltf/tiny_gltf.h"
#define GLM_FORCE_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <filesystem>
#include <iostream>
#include <optional>

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

struct Vertex {
    glm::vec4 ndc_position;
    glm::vec2 texcoord0;
    glm::vec4 world_position;
};

struct Material {
    std::string name;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    glm::vec3 transmittance;
    glm::vec3 emission;
    
    Texture<R8G8B8A8_U>* diffuse_tex;
    Texture<R8G8B8A8_U>* specular_tex;
};

struct LightProbe {
    glm::vec3 position;
    float radius;
    Image<R8G8B8A8_U> irradiance_map;
    Texture<R8G8B8A8_U> radiance_map;
};

struct DrawCall {
    CullMode cull_mode = CullMode::NONE;
    DepthSettings depth_settings = {};
    std::vector<Vertex>* vertex_buffer = nullptr;
    std::vector<std::uint32_t>* index_buffer = nullptr;
    Material* material = nullptr;
    glm::mat4 world_transform = glm::identity<glm::mat4>();
    glm::mat4 vp_transform = glm::identity<glm::mat4>();
    Image<std::uint32_t>* shadow_map;
    glm::mat4 light_mat = glm::identity<glm::mat4>();
};

struct FrameBuffer{
    std::optional<ImageView<R8G8B8A8_U>> color_buffer_view;
    std::optional<ImageView<std::uint32_t>> depth_buffer_view;
};
std::uint32_t get_width(const FrameBuffer* fb){
    if (fb->color_buffer_view.has_value()) return fb->color_buffer_view->width;
    else return fb->depth_buffer_view->width;
}
std::uint32_t get_height(const FrameBuffer* fb){
    if (fb->color_buffer_view.has_value()) return fb->color_buffer_view->height;
    else return fb->depth_buffer_view->height;
}

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
    v.ndc_position = (1.f - t) * v0.ndc_position + t * v1.ndc_position;
    // v.normal = (1.f - t) * v0.normal + t * v1.normal;
    v.texcoord0 = (1.f - t) * v0.texcoord0 + t * v1.texcoord0;
    v.world_position = (1.f - t) * v0.world_position + t * v1.world_position;

    return v;
}

Vertex * clip_triangle(Vertex * triangle, glm::vec4 equation, Vertex * result)
{
    float values[3] =
    {
        glm::dot(triangle[0].ndc_position, equation),
        glm::dot(triangle[1].ndc_position, equation),
        glm::dot(triangle[2].ndc_position, equation),
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

using Plane = glm::vec4;
using Frustum = std::array<Plane, 6>;

inline Frustum extruct_frustum_planes(const glm::mat4& VP) {
    Frustum P;
    P[0] = Plane( VP[0][3] + VP[0][0],
                  VP[1][3] + VP[1][0],
                  VP[2][3] + VP[2][0],
                  VP[3][3] + VP[3][0] );
    P[1] = Plane( VP[0][3] - VP[0][0],
                  VP[1][3] - VP[1][0],
                  VP[2][3] - VP[2][0],
                  VP[3][3] - VP[3][0] );
    P[2] = Plane( VP[0][3] + VP[0][1],
                  VP[1][3] + VP[1][1],
                  VP[2][3] + VP[2][1],
                  VP[3][3] + VP[3][1] );
    P[3] = Plane( VP[0][3] - VP[0][1],
                  VP[1][3] - VP[1][1],
                  VP[2][3] - VP[2][1],
                  VP[3][3] - VP[3][1] );
    P[4] = Plane( VP[0][3] + VP[0][2],
                  VP[1][3] + VP[1][2],
                  VP[2][3] + VP[2][2],
                  VP[3][3] + VP[3][2] );
    P[5] = Plane( VP[0][3] - VP[0][2],
                  VP[1][3] - VP[1][2],
                  VP[2][3] - VP[2][2],
                  VP[3][3] - VP[3][2] );
    for (auto& pl : P) {
        float len = glm::length(glm::vec3(pl));
        pl /= len;
    }
    return P;
}

inline bool is_aabb_outside(const glm::vec3& minB, const glm::vec3& maxB, const Frustum& F) {
    for (const auto& pl : F) {
        glm::vec3 positive;
        positive.x = (pl.x >= 0.f ? maxB.x : minB.x);
        positive.y = (pl.y >= 0.f ? maxB.y : minB.y);
        positive.z = (pl.z >= 0.f ? maxB.z : minB.z);
        if (glm::dot(glm::vec3(pl), positive) + pl.w < 0.f)
            return true;
    }
    return false;
}

bool cull_triangle_by_world_aabb(
    const glm::vec3& v0,
    const glm::vec3& v1,
    const glm::vec3& v2,
    const Frustum& frustum)
{
    glm::vec3 minB = glm::min(v0, glm::min(v1, v2));
    glm::vec3 maxB = glm::max(v0, glm::max(v1, v2));
    return is_aabb_outside(minB, maxB, frustum);
}

struct Uniform{
	glm::mat4 model_mat;
	glm::mat4 proj_view_mat;
	const Material& material;
};

struct VertIn {
	glm::vec4 model_pos;
	glm::vec2 texcoord;
};

struct VertOut {
	glm::vec4 model_pos;
	glm::vec4 world_pos;
	glm::vec4 ndc_pos;
	glm::vec2 texcoord;
};

using FragIn = VertOut;

struct FragOut {
	glm::vec4 color;
	std::uint32_t depth;
};

VertOut vertex_shader(const VertIn& in, const Uniform& uniform){
	glm::vec4 world_pos = uniform.model_mat * in.model_pos;
	glm::vec4 ndc_pos = uniform.proj_view_mat * world_pos;
	return VertOut {
		.model_pos = in.model_pos,
		.world_pos = world_pos,
		.ndc_pos = ndc_pos,
		.texcoord = in.texcoord,
	};
}

FragOut fragment_shader(const FragIn& in, const Uniform& uniform) {
	return FragOut {
		.color = glm::vec4(1.f, 1.f, 1.f, 1.f),
        .depth = 0,
	};
}

void draw(FrameBuffer* frame_buffer, const DrawCall& command, const ViewPort& viewport) {
    const auto frustum = extruct_frustum_planes(command.vp_transform);

    for (std::uint32_t idx_idx = 0; idx_idx + 2 < command.index_buffer->size(); idx_idx+= 3){
        const std::uint32_t i0 = command.index_buffer->at(idx_idx + 0);
        const std::uint32_t i1 = command.index_buffer->at(idx_idx + 1);
        const std::uint32_t i2 = command.index_buffer->at(idx_idx + 2);

        const Vertex v0 = command.vertex_buffer->at(i0);
        const Vertex v1 = command.vertex_buffer->at(i1);
        const Vertex v2 = command.vertex_buffer->at(i2);

        for (std::uint32_t i = 0; i < 3; i++){
            const std::uint32_t idx = command.index_buffer->at(idx_idx + i);
            const Vertex& vert = command.vertex_buffer->at(idx);
            const VertIn vert_input {
                .model_pos = glm::vec4(v0.world_position),
                .texcoord = vert.texcoord0,
            };
        }

        Vertex vertices[12];
        vertices[0] = command.vertex_buffer->at(i0);
        vertices[1] = command.vertex_buffer->at(i1);
        vertices[2] = command.vertex_buffer->at(i2);

        vertices[0].world_position = command.world_transform * vertices[0].world_position;
        vertices[1].world_position = command.world_transform * vertices[1].world_position;
        vertices[2].world_position = command.world_transform * vertices[2].world_position;

        
        if (cull_triangle_by_world_aabb(vertices[0].world_position, vertices[1].world_position, vertices[2].world_position, frustum))
            continue;
        
        auto world_normal = glm::normalize( glm::cross(vertices[1].world_position.xyz - vertices[0].world_position.xyz, vertices[2].world_position.xyz - vertices[0].world_position.xyz));

        vertices[0].ndc_position = command.vp_transform * vertices[0].world_position;
        vertices[1].ndc_position = command.vp_transform * vertices[1].world_position;
        vertices[2].ndc_position = command.vp_transform * vertices[2].world_position;
        
        // this clipping algorithm is taken from https://lisyarus.github.io/blog/posts/implementing-a-tiny-cpu-rasterizer-part-5.html#section-clipping-triangles-implementation
        auto end = clip_triangle(vertices, vertices + 3);

        for (auto triangle_begin = vertices; triangle_begin < end; triangle_begin += 3){
            Vertex v0 = triangle_begin[0];
            Vertex v1 = triangle_begin[1];
            Vertex v2 = triangle_begin[2];
            
            v0.ndc_position = perspective_divide(v0.ndc_position);
            v1.ndc_position = perspective_divide(v1.ndc_position);
            v2.ndc_position = perspective_divide(v2.ndc_position);

            v0.ndc_position = apply(viewport, v0.ndc_position);
            v1.ndc_position = apply(viewport, v1.ndc_position);
            v2.ndc_position = apply(viewport, v2.ndc_position);

            float det012 = det(v1.ndc_position.xy - v0.ndc_position.xy, v2.ndc_position.xy - v0.ndc_position.xy);

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
            std::int32_t xmax = std::min<std::int32_t>(viewport.x + viewport.width, get_width(frame_buffer))-1;
            std::int32_t ymin = std::max<std::int32_t>(viewport.y, 0);
            std::int32_t ymax = std::min<std::int32_t>(viewport.y + viewport.height, get_height(frame_buffer))-1;

            xmin = static_cast<int32_t>(std::max<float>(static_cast<float>(xmin), std::min({std::floor(v0.ndc_position.x), std::floor(v1.ndc_position.x), std::floor(v2.ndc_position.x)})));  
            xmax = static_cast<int32_t>(std::min<float>(static_cast<float>(xmax), std::max({ std::ceil(v0.ndc_position.x), std::ceil(v1.ndc_position.x), std::ceil(v2.ndc_position.x)})));
            ymin = static_cast<int32_t>(std::max<float>(static_cast<float>(ymin), std::min({ std::floor(v0.ndc_position.y), std::floor(v1.ndc_position.y), std::floor(v2.ndc_position.y)})));
            ymax = static_cast<int32_t>(std::min<float>(static_cast<float>(ymax), std::max({ std::ceil(v0.ndc_position.y), std::ceil(v1.ndc_position.y), std::ceil(v2.ndc_position.y)})));

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

                            det01p[dy][dx] = det(v1.ndc_position - v0.ndc_position, p - v0.ndc_position);
                            det12p[dy][dx] = det(v2.ndc_position - v1.ndc_position, p - v1.ndc_position);
                            det20p[dy][dx] = det(v0.ndc_position - v2.ndc_position, p - v2.ndc_position);

                            l0[dy][dx] = det12p[dy][dx] / det012 * v0.ndc_position.w;
                            l1[dy][dx] = det20p[dy][dx] / det012 * v1.ndc_position.w;
                            l2[dy][dx] = det01p[dy][dx] / det012 * v2.ndc_position.w;

                            float lsum = l0[dy][dx] + l1[dy][dx] + l2[dy][dx];

                            l0[dy][dx] /= lsum;
                            l1[dy][dx] /= lsum;
                            l2[dy][dx] /= lsum;

                            auto uv = l0[dy][dx] * v0.texcoord0 + l1[dy][dx] * v1.texcoord0 + l2[dy][dx] * v2.texcoord0;
                            float hoge;
                            uv.x = fabs( modff(uv.x, &hoge));
                            uv.y = fabs(modff(uv.y, &hoge));
                            tex_coord[dy][dx] = uv;
                        }
                    }

                    for (int dy = 0; dy < 2; dy++) {
                        for (int dx = 0; dx < 2; dx++) {
                            if (x + dx > xmax || y + dy > ymax) continue;
                            if (det01p[dy][dx] < 0.f || det12p[dy][dx] < 0.f || det20p[dy][dx] < 0.f) continue;

                            auto ndc_position = l0[dy][dx] * v0.ndc_position + l1[dy][dx] * v1.ndc_position + l2[dy][dx] * v2.ndc_position;
                            auto world_position = l0[dy][dx] * v0.world_position + l1[dy][dx] * v1.world_position + l2[dy][dx] * v2.world_position;
                            
                            if (frame_buffer->depth_buffer_view.has_value()){

                                std::uint32_t depth = static_cast<uint32_t>((0.5f + 0.5f * ndc_position.z) * UINT32_MAX);
    
                                if (!depth_test_passed(command.depth_settings.test_mode, depth, frame_buffer->depth_buffer_view->at(x + dx, y + dy)))
                                    continue;
    
                                if (command.depth_settings.write)
                                    frame_buffer->depth_buffer_view->at(x + dx, y + dy) = depth;
                            }
                            if (frame_buffer->color_buffer_view.has_value()){

                                
                                glm::vec4 color = l0[dy][dx] * v0.ndc_position + l1[dy][dx] * v1.ndc_position + l2[dy][dx] * v2.ndc_position;
                                
                                auto albedo = command.material->diffuse_tex;
                                if (albedo != nullptr){
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
                                    
                                    tc.x = mipmap->width * std::fmod(tex_coord[dy][dx].x, 1.f);
                                    tc.y = mipmap->height * std::fmod(tex_coord[dy][dx].y, 1.f);
                                    
                                    if (mipmap->width == 1 || mipmap->height == 1){
                                        int ix = static_cast<int>(std::floor(tc.x));
                                        int iy = static_cast<int>(std::floor(tc.y));
                                        
                                        ix = std::max(ix, 0);
                                        iy = std::max(iy, 0);
                                        auto texel = mipmap->at(ix, iy);
                                        color = to_vec4(texel);
                                    }
                                    else {
                                        tc.x -= 0.5f;
                                        tc.y -= 0.5f;
                                        
                                        tc.x = std::max<float>(0.f, std::min<float>(mipmap->width - 1.f, tc.x));
                                        tc.y = std::max<float>(0.f, std::min<float>(mipmap->height - 1.f, tc.y));
                                        
                                        int ix = std::min<int>(mipmap->width - 2, static_cast<int>(std::floor(tc.x)));
                                        int iy = std::min<int>(mipmap->height - 2, static_cast<int>(std::floor(tc.y)));
                                        
                                        tc.x -= ix;
                                        tc.y -= iy;
                                        
                                        std::array<glm::vec4, 4>samples = {
                                            to_vec4(mipmap->at(ix + 0, iy + 0)),
                                            to_vec4(mipmap->at(ix + 1, iy + 0)),
                                            to_vec4(mipmap->at(ix + 0, iy + 1)),
                                            to_vec4(mipmap->at(ix + 1, iy + 1)),
                                        };
                                        
                                        color = (1.f - tc.y) * ((1.f - tc.x) * samples[0] + tc.x * samples[1]) + tc.y * ((1.f - tc.x) * samples[2] + tc.x * samples[3]);
                                    }

                                    auto light_space_pos = command.light_mat * world_position;
                                    light_space_pos /= light_space_pos.w;
                                    auto closest_distance = static_cast<float>(command.shadow_map->at(static_cast<std::uint32_t>((light_space_pos.x * 0.5f + 0.5f) * 1024), static_cast<std::uint32_t>((-light_space_pos.y * 0.5f + 0.5f) * 1024))) / UINT32_MAX;
                                    // std::cout << closest_distance << "\n";
                                    auto current_distance = light_space_pos.z * 0.5f + 0.5f;
                                    float shadow_value = current_distance - 0.005f > closest_distance ? 1.f : 0.f;
                                    
                                    frame_buffer->color_buffer_view->at(x + dx, y + dy) = to_r8g8b8a8_u(color * (1.f - shadow_value));
                                    // frame_buffer->color_buffer_view->at(x + dx, y + dy) = to_r8g8b8a8_u(light_space_pos);
                                    // frame_buffer->color_buffer_view->at(x + dx, y + dy) = to_r8g8b8a8_u(glm::vec4(closest_distance));
                                    // frame_buffer->color_buffer_view->at(x + dx, y + dy) = to_r8g8b8a8_u(glm::vec4(current_distance));
                                    // frame_buffer->color_buffer_view->at(x + dx, y + dy) = to_r8g8b8a8_u(glm::vec4(static_cast<float>(closest_distance) / UINT32_MAX));
                                    // frame_buffer->color_buffer_view->at(x + dx, y + dy) = to_r8g8b8a8_u(glm::vec4(world_normal, 1.f));
                                    // frame_buffer->color_buffer_view.at(x + dx, y + dy) = to_r8g8b8a8_u(glm::vec4(uv, 0.f, 1.f));
                                    // frame_buffer->color_buffer_view.at(x + dx, y + dy) = to_r8g8b8a8_u(glm::vec4(glm::vec3(static_cast<float>(mipmap_level) / albedo->mipmaps.size()), 1.f));
                                }
                                else{
                                    frame_buffer->color_buffer_view->at(x + dx, y + dy) = to_r8g8b8a8_u(glm::vec4(command.material->diffuse, 1.f));
                                }
                            }
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
 
Texture<R8G8B8A8_U> load_texture(const std::filesystem::path& path) {
    Texture<R8G8B8A8_U> result{};
    result.mipmaps.push_back(load_image(path));
    generate_mipmaps(&result);
    return result;
}
}