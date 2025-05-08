#pragma once

#include "tinygltf/tiny_gltf.h"
#define GLM_FORCE_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "stb_image/stb_image.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <array>

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

glm::vec4 apply(ViewPort const& vp, glm::vec4 const& vertex);

enum class DepthTestMode{
    NEVER, ALWAYS, LESS, LESSEQUAL, GREATER, GREATEREQUAL, EQUAL, NOTEQUAL,
};

struct DepthSettings{
    bool write = true;
    DepthTestMode test_mode = DepthTestMode::ALWAYS;
};

bool depth_test_passed(DepthTestMode mode, std::uint32_t value, std::uint32_t reference);

template<typename PixelType>
struct Image{
    std::vector<PixelType> image;
    std::uint32_t width, height;
    PixelType& at(std::uint32_t x, std::uint32_t y) {
        return image[y * width + x];
    }
    const PixelType& at(std::uint32_t x, std::uint32_t y) const{
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
    const PixelType& at(std::uint32_t x, std::uint32_t y) const{
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

struct Mesh{
        std::vector<Renderer::Vertex> vertices;
        std::vector<std::uint32_t> indices;
        Renderer::Material material;
};

struct Scene{
    std::vector<Mesh> meshes;
    std::list<Renderer::Texture<Renderer::R8G8B8A8_U>> textures;
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
    glm::vec3 light_direction;
};

struct FrameBuffer{
    std::optional<ImageView<R8G8B8A8_U>> color_buffer_view;
    std::optional<ImageView<std::uint32_t>> depth_buffer_view;
};
std::uint32_t get_width(const FrameBuffer* fb);
std::uint32_t get_height(const FrameBuffer* fb);

template<typename PixelType>
void clear(ImageView<PixelType>* image_view, const PixelType& color) {
    std::fill_n(image_view->image, image_view->width * image_view->height, color);
}

float det(glm::vec2 const& a, glm::vec2 const& b);

using Plane = glm::vec4;
using Frustum = std::array<Plane, 6>;

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

VertOut vertex_shader(const VertIn& in, const Uniform& uniform);
FragOut fragment_shader(const FragIn& in, const Uniform& uniform);

void draw(FrameBuffer* frame_buffer, const DrawCall& command, const ViewPort& viewport);

std::uint32_t bits_reverse( std::uint32_t v );

Renderer::Image<Renderer::R8G8B8A8_U> load_image(std::filesystem::path const& path);

void generate_mipmaps(Renderer::Texture<Renderer::R8G8B8A8_U>* texture);
 
Texture<R8G8B8A8_U> load_texture(const std::filesystem::path& path);
}