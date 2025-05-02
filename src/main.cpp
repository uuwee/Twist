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

void dump_image_to_ppm(Renderer::Image<Renderer::R8G8B8A8_U>& image, std::string const& filename){
    std::ofstream out_File(filename);
    if (!out_File) {
        std::cerr << "Error creating output file." << std::endl;
        return;
    }

    out_File << "P3\n" << image.width << " " << image.height << "\n255\n";
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            auto pixel = image.at(x, y);
            out_File << static_cast<int>(pixel.r) << " " << static_cast<int>(pixel.g) << " " << static_cast<int>(pixel.b) << "\n";
        }
    }
    out_File.close();
}

void dump_image_to_ppm(Renderer::Image<std::uint32_t>& image, std::string const& filename){
    std::ofstream out_File(filename);
    if (!out_File) {
        std::cerr << "Error creating output file." << std::endl;
        return;
    }

    out_File << "P3\n" << image.width << " " << image.height << "\n" << 255 <<"\n";
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            auto pixel = static_cast<double>(image.at(x, y)) / static_cast<double>(UINT32_MAX) * 255;
            out_File << pixel << " 0 0" << "\n";
        }
    }
    out_File.close();
}


int main() {
    std::cout << "hello, world!" << std::endl;
    constexpr int width = 600, height = 400;

    MAC_greet();
    MAC::Image<std::uint32_t> test_mac_image{
        .data = nullptr,
        .width = 0,
        .height = 0,
    };
    std::cout << sizeof(test_mac_image) << "\n";

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

    auto brick_texture = Renderer::load_texture("./resource/brick_1024.jpg");

    auto box_mesh = Primitives::create_cube();

    ModelLoader::Scene scene;
    ModelLoader::load_scene(&scene, "./resource/sibenik/sibenik.obj");

    Renderer::R8G8B8A8_U clear_color = {255, 200, 200, 255};

    // timer
    auto last_frame_start = std::chrono::high_resolution_clock::now();

    // state
    bool running = true;
    bool dump_image = false;

    glm::vec3 camera_pos = {0.f, 0.f, -1.f};
    float y_rotation = 0.f;
    glm::vec2 mouse_pos = {0.f, 0.f};

    // shadow pass
    glm::vec3 light_dir = {0.f, 0.f, 0.f};
    glm::vec3 light_pos = {10.f, 50.f, 10.f};
    clear(shadow_map_view, 0xFFFFFFFF);

    Renderer::ViewPort shadow_viewport{
        .x = 0,
        .y = 0, 
        .width = shadow_map_width,
        .height = shadow_map_height,
    };
    auto shadow_proj = glm::ortho<float>(-50, 50, -50, 50, 0.1f, 100.f);
    auto shadow_view = glm::lookAt(light_pos, light_dir, glm::vec3(1.f, 0.f, 0.f));
    auto model_mat = glm::identity<glm::mat4>();
        model_mat = glm::scale(model_mat, glm::vec3(1.f, 1.f, 1.f));
        model_mat = glm::translate(model_mat, glm::vec3(0.f, 0.f, -2.f));
    for (auto& mesh: scene.meshes){
        Renderer::Material mat {
            .ambient = glm::vec3(mesh.material.ambient.at(0), mesh.material.ambient.at(1), mesh.material.ambient.at(2)),
            .diffuse = glm::vec3(mesh.material.diffuse.at(0), mesh.material.diffuse.at(1), mesh.material.diffuse.at(2)),
            .specular = glm::vec3(mesh.material.specular.at(0), mesh.material.specular.at(1), mesh.material.specular.at(2)),
            .transmittance = glm::vec3(mesh.material.transmittance.at(0), mesh.material.transmittance.at(1), mesh.material.transmittance.at(2)),
            .emission = glm::vec3(mesh.material.emission.at(0), mesh.material.emission.at(1), mesh.material.emission.at(2)),
            .diffuse_tex = mesh.texture.has_value() ? &mesh.texture.value() : nullptr,
        };
        bool is_transparant = glm::length2(mat.transmittance) < 0.99f;
        if (is_transparant) continue;
        draw(
            &shadow_frame_buffer,
            {
                .cull_mode = Renderer::CullMode::NONE,
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
            Renderer::Material mat {
                .ambient = glm::vec3(mesh.material.ambient.at(0), mesh.material.ambient.at(1), mesh.material.ambient.at(2)),
                .diffuse = glm::vec3(mesh.material.diffuse.at(0), mesh.material.diffuse.at(1), mesh.material.diffuse.at(2)),
                .specular = glm::vec3(mesh.material.specular.at(0), mesh.material.specular.at(1), mesh.material.specular.at(2)),
                .transmittance = glm::vec3(mesh.material.transmittance.at(0), mesh.material.transmittance.at(1), mesh.material.transmittance.at(2)),
                .emission = glm::vec3(mesh.material.emission.at(0), mesh.material.emission.at(1), mesh.material.emission.at(2)),
                .diffuse_tex = mesh.texture.has_value() ? &mesh.texture.value() : nullptr,
            };
            bool is_transparant = glm::length2(mat.transmittance) < 0.99f;
            if (is_transparant) continue;
            draw(
                &frame_buffer, 
                {
                    .cull_mode = Renderer::CullMode::NONE,
                    .depth_settings = {
                        .write = true,
                        .test_mode = Renderer::DepthTestMode::LESS,
                    },
                    .vertex_buffer = &mesh.vertices,
                    .index_buffer = &mesh.indices,
                    .material = &mat,
                    .world_transform = model_mat,
                    .vp_transform = proj_mat * view_mat,
                    .shadow_map = &shadow_map,
                    .light_mat = shadow_proj * shadow_view,
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
            dump_image_to_ppm(shadow_map, "./bin/shadow_map.ppm");
            dump_image = false;
        }
    };

    return 0;
}