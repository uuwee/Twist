#include "tinygltf/tiny_gltf.h"
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

struct R8G8B8A8_U{
    std::uint8_t r, g, b, a;
};
static inline uint32_t ToUint32(R8G8B8A8_U color) {
    return color.r << 24 | color.g << 16 | color.b << 8 | color.a << 0;
}
static inline R8G8B8A8_U to_color4ub(glm::vec4 const & color) {
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
    std::vector<uint32_t> index;
};

uint32_t bits_reverse( uint32_t v )
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
    We input R8G8B8A8, but SDL_Surface.pixels is sumehow ABGR8G8R8*.
    */

    out_File << "P3\n" << surface.w << " " << surface.h << "\n255\n";
    for (int y = 0; y < surface.h; ++y) {
        for (int x = 0; x < surface.w; ++x) {
            uint32_t pixel = ((uint32_t*)surface.pixels)[y * surface.w + x];
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

    Mesh mesh = {
        .vertex = {
            {-0.5f, -0.5f, -0.5f},
            { 0.5f, -0.5f, -0.5f},
            { 0.5f,  0.5f, -0.5f},
            {-0.5f,  0.5f, -0.5f},
            {-0.5f, -0.5f,  0.5f},
            { 0.5f, -0.5f,  0.5f},
            { 0.5f,  0.5f,  0.5f},
            {-0.5f,  0.5f,  0.5f},
        },
        .index = {
            0, 1, 2,
            0, 2, 3,
            4, 5, 6,
            4, 6, 7,
            0, 1, 5,
            0, 5, 4,
            1, 2, 6,
            1, 6, 5,
            2, 3, 7,
            2, 7, 6,
            3, 0, 4,
            3, 4, 7
        },
    };
    glm::mat4 model_matrix = glm::identity<glm::mat4>();
    glm::mat4 view_matrix = glm::identity<glm::mat4>();
    glm::mat4 projection_matrix = glm::perspective(glm::radians(45.f), static_cast<float>(width) / height, 0.1f, 100.f);
    projection_matrix[1][1] *= -1.f; // flip y axis
    
    R8G8B8A8_U clear_color = {255, 200, 200, 255};

    // pixel indices to be used in parallel for_each
    int* pixel_indices = new int[width * height];
    std::iota(pixel_indices, pixel_indices + width * height, 0);

    // timer
    auto last_frame_start = std::chrono::high_resolution_clock::now();

    // state
    bool running = true;
    bool dump_image = false;

    glm::vec3 camera_pos = {0.f, 0.f, -1.f};
    float y_rotation = 0.f;

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
        
        default:
            break;
        }

        if (!running) break;

        auto now = std::chrono::high_resolution_clock::now();
        auto delta_time = std::chrono::duration<float>(now - last_frame_start).count();
        last_frame_start = now;

        // std::cout << "delta_time: " << delta_time << std::endl;
        std::cout << "FPS: " << 1.0f / delta_time << std::endl;

        // update camera
        // auto view_matrix = glm::translate(glm::identity<glm::mat4>(), -camera_pos);
        auto view_matrix = glm::lookAt(
            camera_pos,
            glm::vec3(0.f, 0.f, 0.f),
            glm::vec3(0.f, 1.f, 0.f)
        );
        auto rotation_matrix = glm::rotate(glm::identity<glm::mat4>(), y_rotation, glm::vec3(0.f, 1.f, 0.f));
        y_rotation += delta_time * 0.5f; // rotate 0.5 radian per second
        auto mvp_matrix = projection_matrix * view_matrix * rotation_matrix * model_matrix;
        
        // clear color
        std::fill_n((R8G8B8A8_U*) draw_surface->pixels, width * height, clear_color);
        
        // draw
        std::for_each(std::execution::par_unseq, pixel_indices, pixel_indices + width * height, [&draw_surface, &clear_color, &mesh, mvp_matrix](int idx){
            const int s = idx % width;
            const int t = idx / width;
            const float u = static_cast<float>(s) / width * 2.f - 1.f;
            const float v = static_cast<float>(t) / height * 2.f - 1.f;

            for (std::uint32_t vert_idx = 0; vert_idx + 2 < mesh.index.size(); vert_idx += 3) {
                auto v0 = mvp_matrix * glm::vec4(mesh.vertex[mesh.index[vert_idx + 0]], 1.f);
                auto v1 = mvp_matrix * glm::vec4(mesh.vertex[mesh.index[vert_idx + 1]], 1.f);
                auto v2 = mvp_matrix * glm::vec4(mesh.vertex[mesh.index[vert_idx + 2]], 1.f);

                v0 /= v0.w; v1 /= v1.w; v2 /= v2.w;

                glm::vec4 p = glm::vec4(u, v, 0.f, 0.f);
                auto det = [](glm::vec4 a, glm::vec4 b) {
                    return a.x * b.y - a.y * b.x;
                };
                glm::vec4 b = glm::vec4(
                    det(v1 - v0, p - v0),
                    det(v2 - v1, p - v1),
                    det(v0 - v2, p - v2),
                    0.f
                );
                float area = glm::dot(b, glm::vec4(1.f, 1.f, 1.f, 0.f));
                // if (area < 0.f) continue; // backface culling
                b /= area; // barycentric coordinates
                float alpha = b.x;
                float beta = b.y;
                float gamma = b.z;
                // if (alpha > 1.f || beta > 1.f || gamma > 1.f) continue;
                if (alpha < 0.f || beta < 0.f || gamma < 0.f) continue;


                // interpolate color
                auto color = glm::vec4(
                    alpha,
                    beta,
                    gamma,
                    1.f
                );
                
                // draw pixel
                ((R8G8B8A8_U*)draw_surface->pixels)[idx] = to_color4ub(color);
            }

            // ((R8G8B8A8_U*)draw_surface->pixels)[idx] = to_color4ub(glm::vec4(
            //     static_cast<float>(s) / width,
            //     static_cast<float>(t) / height,
            //     0.5f,
            //     1.0f
            // ));
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

    // constexpr float max_value = 255.0;
    // std::string header = "P3\n" + std::to_string(width) + " " + std::to_string(height) + "\n255\n";

    // std::ofstream outFile("./bin/output.ppm");
    // if (!outFile) {
    //     std::cerr << "Error creating output file." << std::endl;
    //     return 1;
    // }
    // outFile << header;
    // for (int i = 0; i < height; ++i) {
    //     for (int j = 0; j < width; ++j) {
    //         int idx = i * width + j;
    //         outFile << static_cast<int>(image[idx][0] / 1.0 * max_value)
    //             << " " << static_cast<int>(image[idx][1] / 1.0 * max_value)
    //             << " " << static_cast<int>(image[idx][2] / 1.0 * max_value) << "\n";
    //     }
    // }
    // outFile.close();

    delete [] pixel_indices;
    // delete[] image;

    return 0;
}