#include "tinygltf/tiny_gltf.h"
#define GLM_FORCE_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "stb_image/stb_image.h"

#include "renderer/renderer.hpp"
#include "utils/primitive.hpp"

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

Image<R8G8B8A8_U> load_image(std::filesystem::path const& path) {
    uint32_t width, height;
    int channels;
    char path_char[1024];
    size_t size;
    wcstombs_s(&size, path_char, path.c_str(), path.string().size());
    // wcstombs(path_char, path.c_str(), path.string().size());
    R8G8B8A8_U* data = (R8G8B8A8_U*) stbi_load(path_char, (int*)&width, (int*)&height, &channels, 4);
    std::cout << "load file:" << path << ", size=" << width << "x" << height << std::endl;
    Image<R8G8B8A8_U> result {
        .image = std::vector<R8G8B8A8_U>(data, data + width * height),
        .width = width, 
        .height = height,
    };
    return result;
}

void generate_mipmaps(Texture<R8G8B8A8_U>* texture){
    if (texture->mipmaps.empty()) return;

    texture->mipmaps.resize(1);

    for (int i = 1;;i++){
        auto& prev_level = texture->mipmaps[i - 1];

        if (prev_level.width == 1 && prev_level.height == 1)
            break;

        std::uint32_t new_width = prev_level.width / 2 + (prev_level.width & 1);
        std::uint32_t new_height = prev_level.height / 2 + (prev_level.height & 1);

        Image<R8G8B8A8_U> next_level = {
            .image = std::vector<R8G8B8A8_U>(new_width * new_height),
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

                next_level.at(x, y) = to_r8g8b8a8_u(result);
            }
        }

        texture->mipmaps.push_back(std::move(next_level));
    }
}

int main() {
    std::cout << "hello, world!" << std::endl;
    constexpr int width = 600, height = 400;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Twist", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    SDL_Surface* draw_surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_SetSurfaceBlendMode(draw_surface, SDL_BLENDMODE_NONE);

    ImageView<R8G8B8A8_U> render_target_view = {
        .image = (R8G8B8A8_U*)draw_surface->pixels,
        .width = width,
        .height = height,
    };

    Image<std::uint32_t> depth_buffer = {
        .image = std::vector<std::uint32_t>(width * height),
        .width = width, 
        .height = height,
    };
    ImageView<std::uint32_t> depth_buffer_view = create_imageview(depth_buffer, width, height);

    FrameBuffer frame_buffer = {
        .color_buffer_view = render_target_view,
        .depth_buffer_view = depth_buffer_view,
    };

    Mesh mesh = Primitives::create_cube();

    // Texture<R8G8B8A8_U> brick_texture{
    //     .mipmaps = 
    // }
    std::filesystem::path brick_img_path = "./resource/brick_1024.jpg";
    Texture<R8G8B8A8_U> brick_texture{};
    brick_texture.mipmaps.push_back(load_image(brick_img_path));
    generate_mipmaps(&brick_texture);

    
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
        clear(depth_buffer_view, 0xFFFFFFFF);

        ViewPort viewport = {
            .x = 0,
            .y = 0,
            .width = width,
            .height = height,
        };

        y_rotation += delta_time;
        auto model_mat = glm::identity<glm::mat4>();
        model_mat = glm::translate(model_mat, glm::vec3(0.f, 0.f, -2.f));
        model_mat = glm::rotate(model_mat, y_rotation, glm::vec3(0.f, 1.f, 0.f));
        model_mat = glm::rotate(model_mat, y_rotation*0.1f, glm::vec3(1.f, 1.f, 0.f));
        auto view_mat = glm::identity<glm::mat4>();
        view_mat = glm::translate(view_mat, camera_pos);
        auto proj_mat = glm::perspective(glm::radians(90.0f), static_cast<float>(width) / height, 0.1f, 100.f);
        draw(
            &frame_buffer, 
            {
                .cull_mode = CullMode::NONE,
                .depth_settings = {
                    .write = true,
                    .test_mode = DepthTestMode::LESS,
                },
                .mesh = &mesh,
                .texture = &brick_texture,
                .transform = proj_mat * view_mat * model_mat,
            },
            viewport  
        );

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