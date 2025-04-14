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

struct color4ub{
    std::uint8_t r, g, b, a;
};
static inline uint32_t ToUint32(color4ub color) {
    return color.r << 24 | color.g << 16 | color.b << 8 | color.a << 0;
}
static inline color4ub to_color4ub(glm::vec4 const & color) {
    return {
        .r = (uint8_t)std::max(0.f, std::min(255.f, color.x * 255.f)),
        .g = (uint8_t)std::max(0.f, std::min(255.f, color.y * 255.f)),
        .b = (uint8_t)std::max(0.f, std::min(255.f, color.z * 255.f)),
        .a = (uint8_t)std::max(0.f, std::min(255.f, color.w * 255.f)),
    };
}

int main() {
    std::cout << "hello, world!" << std::endl;
    constexpr int width = 600, height = 400;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Twist", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    SDL_Surface* draw_surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_SetSurfaceBlendMode(draw_surface, SDL_BLENDMODE_NONE);
    
    color4ub clear_color = {255, 200, 200, 255};

    int* pixel_indices = new int[width * height];
    std::iota(pixel_indices, pixel_indices + width * height, 0);

    bool running = true;
    while(running) {
        for (SDL_Event event; SDL_PollEvent(&event); ) switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        
        default:
            break;
        }

        if (!running) break;
        
        // clear color
        std::fill_n((color4ub*) draw_surface->pixels, width * height, clear_color);
        
        // draw
        std::for_each(std::execution::par_unseq, pixel_indices, pixel_indices + width * height, [draw_surface, clear_color](int idx){
            int s = idx % width;
            int t = idx / width;

            ((color4ub*)draw_surface->pixels)[idx] = to_color4ub(glm::vec4(
                static_cast<float>(s) / width,
                static_cast<float>(t) / height,
                0.5f,
                1.0f
            ));
        });

        SDL_Rect rect{
            .x = 0, .y = 0, .w = width, .h = height
        };
        SDL_BlitSurface(draw_surface, &rect, SDL_GetWindowSurface(window), &rect);
        SDL_UpdateWindowSurface(window);
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

    // delete [] pixel_indices;
    // delete[] image;

    return 0;
}