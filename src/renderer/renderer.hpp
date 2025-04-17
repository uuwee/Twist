#pragma once

#include "tinygltf/tiny_gltf.h"
#define GLM_FORCE_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include <cstdint>
#include <array>


namespace Renderer
{
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
    
} // namespace Renderer