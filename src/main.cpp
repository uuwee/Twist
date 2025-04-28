#define GLM_FORCE_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "stb_image/stb_image.h"
#include "rapidobj/rapidobj.hpp"

#include "renderer/renderer.hpp"
#include "utils/primitive.hpp"
#include "utils/model_loader.hpp"

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


int main() {
    std::cout << "hello, world!" << std::endl;
    constexpr int width = 600, height = 400;

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
    Renderer::ImageView<std::uint32_t> depth_buffer_view = create_imageview(depth_buffer, width, height);

    Renderer::FrameBuffer frame_buffer = {
        .color_buffer_view = render_target_view,
        .depth_buffer_view = depth_buffer_view,
    };

    std::filesystem::path brick_img_path = "./resource/brick_1024.jpg";
    Renderer::Texture<Renderer::R8G8B8A8_U> brick_texture{};
    brick_texture.mipmaps.push_back(Renderer::load_image(brick_img_path));
    Renderer::generate_mipmaps(&brick_texture);

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
        auto model_mat = glm::identity<glm::mat4>();
        model_mat = glm::scale(model_mat, glm::vec3(1.f, 1.f, 1.f));
        model_mat = glm::translate(model_mat, glm::vec3(0.f, 0.f, -2.f));
        model_mat = glm::rotate(model_mat, y_rotation, glm::vec3(0.f, 1.f, 0.f));
        model_mat = glm::rotate(model_mat, y_rotation*0.1f, glm::vec3(1.f, 1.f, 0.f));
        auto view_mat = glm::identity<glm::mat4>();
        view_mat = glm::translate(view_mat, camera_pos);
        auto proj_mat = glm::perspective(glm::radians(90.0f), static_cast<float>(width) / height, 0.1f, 100.f);

        for (auto& mesh : scene.meshes) {
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
                    .texture = mesh.texture.has_value() ? &mesh.texture.value() : &brick_texture,
                    .transform = proj_mat * view_mat * model_mat,
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
            dump_surface_to_ppm(*draw_surface);
            dump_image = false;
        }
    };

    return 0;
}