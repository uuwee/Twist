#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/norm.hpp"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "stb_image/stb_image.h"
#include "rapidobj/rapidobj.hpp"

#include "renderer/renderer.hpp"
#include "utils/primitive.hpp"
#include "utils/model_loader.hpp"
#include "macaroni/rasterizer.h"
#include "utils/image_io.hpp"

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
#include <filesystem>

using namespace Renderer;

struct LightProbe {
    glm::vec3 position;
    std::array<Image<R8G8B8A8_U>, 6> irradiance_map;
    std::array<Texture<R8G8B8A8_U>, 6> radiance_map;
};

void draw_light_probe(const Scene* scene, LightProbe* probe){
    const glm::vec3 position = probe->position;

    
}

int main() {
    std::cout << "hello, world!" << std::endl;
    constexpr int width = 600, height = 400;

    MAC_greet();

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Twist", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    SDL_Surface* draw_surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_SetSurfaceBlendMode(draw_surface, SDL_BLENDMODE_NONE);

    Renderer::ImageView<Renderer::R8G8B8A8_U> render_target_view = {
        .image = (Renderer::R8G8B8A8_U*)draw_surface->pixels,
        .width = width,
        .height = height,
    };

    Renderer::Image<std::uint32_t> depth_buffer = {
        .image = std::vector<std::uint32_t>(width * height),
        .width = width, 
        .height = height,
    };
    auto depth_buffer_view = create_imageview(depth_buffer, width, height);

    Renderer::FrameBuffer frame_buffer = {
        .color_buffer_view = render_target_view,
        .depth_buffer_view = depth_buffer_view,
    };

    constexpr int shadow_map_width = 1024, shadow_map_height = 1024;
    Renderer::Image<std::uint32_t> shadow_map = {
        .image = std::vector<std::uint32_t>(shadow_map_width * shadow_map_height),
        .width = shadow_map_width,
        .height = shadow_map_height,
    };
    auto shadow_map_view = create_imageview(shadow_map, shadow_map_width, shadow_map_height);
    Renderer::FrameBuffer shadow_frame_buffer = {
        .color_buffer_view = std::nullopt,
        .depth_buffer_view = shadow_map_view,
    };

    auto box_mesh = Primitives::create_cube();

    Renderer::Scene scene;
    ModelLoader::load_scene(&scene, "./resource/sibenik/sibenik.obj");
    // ModelLoader::load_scene(&scene, "./resource/camera/camera.obj");

    Renderer::R8G8B8A8_U clear_color = {255, 200, 200, 255};

    // timer
    auto last_frame_start = std::chrono::high_resolution_clock::now();

    // state
    bool running = true;
    bool dump_image = false;
    float time = 0.f;

    glm::vec3 camera_pos = {0.f, 0.f, -1.f};
    float y_rotation = 0.f;
    glm::vec2 mouse_pos = {0.f, 0.f};
    float camera_speed = 1.f;

    // shadow pass
    glm::vec3 light_lookat = {0.f, 0.f, 0.f};
    glm::vec3 light_pos = {10.f, 50.f, -50.f};
    clear(shadow_map_view, 0xFFFFFFFF);

    Renderer::ViewPort shadow_viewport{
        .x = 0,
        .y = 0, 
        .width = shadow_map_width,
        .height = shadow_map_height,
    };
    auto shadow_proj = glm::ortho<float>(-50, 50, -50, 50, 0.1f, 100.f);
    auto shadow_view = glm::lookAt(light_pos, light_lookat, glm::vec3(1.f, 0.f, 0.f));
    auto model_mat = glm::identity<glm::mat4>();
        model_mat = glm::scale(model_mat, glm::vec3(1.f, 1.f, 1.f));
        model_mat = glm::translate(model_mat, glm::vec3(0.f, 0.f, -2.f));
    for (auto& mesh: scene.meshes){
        bool is_transparant = glm::length2(mesh.material.transmittance) < 0.99f;
        if (is_transparant) continue;
        draw(
            &shadow_frame_buffer,
            {
                .cull_mode = Renderer::CullMode::CLOCK_WISE,
                .depth_settings = {
                    .write = true,
                    .test_mode = Renderer::DepthTestMode::LESS,
                },
                .vertex_buffer = &mesh.vertices,
                .index_buffer = &mesh.indices,
                .material = nullptr,
                .world_transform = model_mat,
                .vp_transform = shadow_proj * shadow_view,
            },
            shadow_viewport
        );
    }

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
                dump_image = true;
                break;
            case SDL_KeyCode::SDLK_w:
                camera_pos.y -= camera_speed;
                break;
            case SDL_KeyCode::SDLK_s:
                camera_pos.y += camera_speed;
                break;
            case SDL_KeyCode::SDLK_a:
                camera_pos.x += camera_speed;
                break;
            case SDL_KeyCode::SDLK_d:
                camera_pos.x -= camera_speed;
                break;
            case SDL_KeyCode::SDLK_q:
                camera_pos.z += camera_speed;
                break;
            case SDL_KeyCode::SDLK_e:
                camera_pos.z -= camera_speed;
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
        time += delta_time;
        last_frame_start = now;

        std::ostringstream title;
        title << "FPS:" << 1.f / delta_time;
        SDL_SetWindowTitle(window, title.str().c_str());
        
        // clear color
        clear(render_target_view, clear_color);  
        clear(depth_buffer_view, 0xFFFFFFFF);

        Renderer::ViewPort viewport = {
            .x = 0,
            .y = 0,
            .width = width,
            .height = height,
        };

        y_rotation += delta_time;
        auto view_mat = glm::identity<glm::mat4>();
        view_mat = glm::translate(view_mat, camera_pos);
        auto proj_mat = glm::perspective(glm::radians(90.0f), static_cast<float>(width) / height, 0.1f, 100.f);

        for (auto& mesh : scene.meshes) {
            // remove glasses
            bool is_transparant = glm::length2(mesh.material.transmittance) < 0.99f;
            if (is_transparant) continue;

            draw(
                &frame_buffer, 
                {
                    .cull_mode = Renderer::CullMode::CLOCK_WISE,
                    .depth_settings = {
                        .write = true,
                        .test_mode = Renderer::DepthTestMode::LESS,
                    },
                    .vertex_buffer = &mesh.vertices,
                    .index_buffer = &mesh.indices,
                    .material = &mesh.material,
                    .world_transform = model_mat,
                    .vp_transform = proj_mat * view_mat,
                    .shadow_map = &shadow_map,
                    .light_mat = shadow_proj * shadow_view,
                    .light_direction = glm::normalize(light_lookat - light_pos),
                },
                viewport  
            );
        }

        SDL_Rect rect{
            .x = 0, .y = 0, .w = width, .h = height
        };
        SDL_BlitSurface(draw_surface, &rect, SDL_GetWindowSurface(window), &rect);
        SDL_UpdateWindowSurface(window);

        if (dump_image) {
            ImageIO::dump_image_to_ppm(shadow_map, "./bin/shadow_map.ppm");
            dump_image = false;
        }
    };

    return 0;
}