#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/norm.hpp"
#include <glm/gtx/string_cast.hpp>
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

enum class CubeMapIndex : std::uint32_t {
    UP = 0,
    DOWN = 1,
    LEFT = 2,
    RIGHT = 3,
    FRONT = 4, 
    BACK = 5,
};

static glm::mat4 get_cube_map_view_proj_matrix(const CubeMapIndex idx, const glm::vec3& position) {
    glm::vec3 look_pos = position - glm::vec3(0.f, 1.f, 0.f);
    glm::vec3 up = glm::vec3(1.f, 0.f, 0.f);

    switch(idx) {
    case CubeMapIndex::UP:
        look_pos = position - glm::vec3(0.f, 1.f, 0.f);
        up = glm::vec3(1.f, 0.f, 0.f);
        break;
    case CubeMapIndex::DOWN:
        look_pos = position + glm::vec3(0.f, 1.f, 0.f);
        up = glm::vec3(1.f, 0.f, 0.f);
        break;
    case CubeMapIndex::LEFT:
        look_pos = position - glm::vec3(1.f, 0.f, 0.f);
        up = glm::vec3(0.f, 1.f, 0.f);
        break;
    case CubeMapIndex::RIGHT:
        look_pos = position + glm::vec3(1.f, 0.f, 0.f);
        up = glm::vec3(0.f, 1.f, 0.f);
        break;
    case CubeMapIndex::FRONT:
        look_pos = position - glm::vec3(0.f, 0.f, 1.f);
        up = glm::vec3(1.f, 0.f, 0.f);
        break;
    case CubeMapIndex::BACK:
        look_pos = position + glm::vec3(0.f, 0.f, 1.f);
        up = glm::vec3(1.f, 0.f, 0.f);
        break;
    }

    const auto view = glm::lookAt(position, look_pos, up);
    const auto proj = glm::perspective(glm::radians(90.0f), 1.f, 0.1f, 100.f);
    return proj * view;
}

struct LightProbe {
    glm::vec3 position;
    std::uint32_t resolution = 128;
    std::array<Renderer::Image<R8G8B8A8_U>, 6> irradiance_map;
    std::array<Texture<R8G8B8A8_U>, 6> radiance_map;
};

void init_light_probe(LightProbe* probe, glm::vec3 position) {
    probe->position = position;
    const std::uint32_t resolution = probe->resolution;

    for (std::uint32_t i = static_cast<std::uint32_t>(CubeMapIndex::UP); i <= static_cast<std::uint32_t>(CubeMapIndex::BACK); i++){
        const CubeMapIndex idx = static_cast<CubeMapIndex>(i);
        probe->irradiance_map.at(i) = Image<R8G8B8A8_U>{
            .image = std::vector<R8G8B8A8_U>(resolution * resolution),
            .width = resolution,
            .height = resolution,
        };
        probe->radiance_map.at(i).mipmaps.clear();
        probe->radiance_map.at(i).mipmaps.push_back(
            Image<Renderer::R8G8B8A8_U>{
                .image = std::vector<R8G8B8A8_U>(resolution * resolution),
                .width = resolution,
                .height = resolution,
            }
        );    
        auto view = create_imageview(probe->radiance_map.at(i).mipmaps.back(), probe->resolution, probe->resolution);
        clear(&view, R8G8B8A8_U(0, 255, 0, 255));
    }
}

void dump_light_probe(const LightProbe& probe, const std::filesystem::path& output_dir){
    for (std::uint32_t i = static_cast<std::uint32_t>(CubeMapIndex::UP); i <= static_cast<std::uint32_t>(CubeMapIndex::BACK); i++){
        const std::filesystem::path path = output_dir / "rad" / std::to_string(i);
        const Renderer::Texture<Renderer::R8G8B8A8_U> tex = probe.radiance_map.at(i);
        ImageIO::dump_texture_to_ppm( tex, path);
    }
}

glm::vec3 env_map_direction(size_t image_idx, glm::vec2 st){

    float u = 2.0f * st.x - 1.0f;
    float v = 2.0f * st.y - 1.0f;

    glm::vec3 direction;

    switch (static_cast<CubeMapIndex>(image_idx)) {
        case CubeMapIndex::RIGHT:
            direction = glm::vec3(1.0f, v, -u);
            break;
        case CubeMapIndex::LEFT:
            direction = glm::vec3(-1.0f, v, u);
            break;
        case CubeMapIndex::UP:
            direction = glm::vec3(u, 1.0f, -v);
            break;
        case CubeMapIndex::DOWN:
            direction = glm::vec3(u, -1.0f, v);
            break;
        case CubeMapIndex::FRONT:
            direction = glm::vec3(u, v, 1.0f);
            break;
        case CubeMapIndex::BACK:
            direction = glm::vec3(-u, v, -1.0f);
            break;
        default:
            direction = glm::vec3(0.0f);
            break;
    }    return glm::normalize(direction);
};

void diffuse_convolution (LightProbe* output_env_map, LightProbe* input_env_map){
    if ((output_env_map->resolution != input_env_map->resolution)) {
        std::cerr << "failed to convolut diffuse map\n";
        return;
    }

    for (size_t i = 0; i < 6; i++){
        std::cout << "convoluting env map face:" << i << "\n";
        for (std::uint32_t y = 0; y < output_env_map->resolution; y++)
        for (std::uint32_t x = 0; x < output_env_map->resolution; x++){
            glm::vec4 sum = glm::vec4(0.f);
            glm::vec3 N = env_map_direction( i, glm::vec2((float)y / output_env_map->resolution, (float)x / output_env_map->resolution));

            for (size_t j = 0; j < 6; j++){
                // std::cout << "gathering env map face:" << j << "\n";
                for (std::uint32_t y1 = 0; y1 < input_env_map->resolution; y1++)
                for (std::uint32_t x1 = 0; x1 < input_env_map->resolution; x1++){
                    glm::vec3 L = env_map_direction( j, glm::vec2((float) y1 / input_env_map->resolution, (float) x1 / output_env_map->resolution));
                    glm::vec4 I = to_vec4(input_env_map->radiance_map.at(i).mipmaps[0].at(x1, y1));
                    sum += std::max(0.f, glm::dot(L, N)) * I;
                }
            }

            output_env_map->radiance_map.at(i).mipmaps[0].at(x, y) = Renderer::to_r8g8b8a8_u(sum);
        }
    }
}

void draw_light_probe(LightProbe* probe, Scene& scene, Image<std::uint32_t>& shadow_map, const glm::mat4& light_mat, const glm::vec3& light_dir){
    const glm::vec3 position = probe->position;

    Renderer::Image<std::uint32_t> depth_buffer{
        .image = std::vector<std::uint32_t>(probe->resolution * probe->resolution),
        .width = probe->resolution,
        .height = probe->resolution,
    };
    auto depth_buffer_view = create_imageview(depth_buffer, probe->resolution, probe->resolution);

    for (std::uint32_t i = static_cast<std::uint32_t>(CubeMapIndex::UP); i <= static_cast<std::uint32_t>(CubeMapIndex::BACK); i++) {
        clear(&depth_buffer_view, 0xFFFFFFFF);

        auto rad_map_img_view = create_imageview(probe->radiance_map.at(i).mipmaps.at(0), probe->resolution, probe->resolution);
        clear(&rad_map_img_view, R8G8B8A8_U(255, 0, 0, 255));

        Renderer::FrameBuffer frame_buffer = {
            .color_buffer_view = rad_map_img_view,
            .depth_buffer_view = depth_buffer_view,
        };

        const glm::mat4 vp_mat = get_cube_map_view_proj_matrix(static_cast<CubeMapIndex>(i), position);
        std::cout << "probe pos: " << glm::to_string(probe->position) << "\n";
        const Renderer::ViewPort viewport {
            .x = 0, .y = 0, .width = probe->resolution, .height = probe->resolution,
        };

        for (auto& mesh : scene.meshes) {
            Renderer::DrawCall call {
                .cull_mode = Renderer::CullMode::CLOCK_WISE,
                .depth_settings = {
                    .write = true,
                    .test_mode = Renderer::DepthTestMode::LESS,
                },
                .vertex_buffer = &mesh.vertices,
                .index_buffer = &mesh.indices,
                .material = &mesh.material,
                .world_transform = glm::identity<glm::mat4>(),
                .vp_transform = vp_mat,
                .shadow_map = &shadow_map,
                .light_mat = light_mat,
                .light_direction = light_dir,
            };
            Renderer::draw(
                &frame_buffer, 
                call,
                viewport
            );
        }

        Renderer::generate_mipmaps(&probe->radiance_map.at(i));
    }

    std::cout << "finished radiance map";
    // dump_light_probe(*probe, "./bin/probes");
    LightProbe rad_env_map{};
    init_light_probe(&rad_env_map, glm::vec3(0.f, 0.1f, 0.f));
    diffuse_convolution (&rad_env_map, probe);
    for (int i = 0; i < 6; i++) Renderer::generate_mipmaps(&rad_env_map.radiance_map.at(i));
    dump_light_probe(rad_env_map, "./bin/probes");
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

    constexpr int shadow_map_width = 2048, shadow_map_height = 2048;
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

    LightProbe probe = {};
    init_light_probe(&probe, glm::vec3(0.f, 0.1f, 0.f));
    

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
    clear(&shadow_map_view, 0xFFFFFFFF);

    Renderer::ViewPort shadow_viewport{
        .x = 0,
        .y = 0, 
        .width = shadow_map_width,
        .height = shadow_map_height,
    };
    auto shadow_proj = glm::ortho<float>(-50, 50, -50, 50, 0.1f, 100.f);
    auto shadow_view = glm::lookAt(light_pos, light_lookat, glm::vec3(1.f, 0.f, 0.f));
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
                .world_transform = glm::identity<glm::mat4>(),
                .vp_transform = shadow_proj * shadow_view,
            },
            shadow_viewport
        );
    }

    // light probe pass
    const glm::mat4 light_mat = shadow_proj * shadow_view;
    const glm::vec3 light_dir = glm::normalize(light_lookat - light_pos);
    draw_light_probe(&probe, scene, shadow_map, light_mat, light_dir);
    // dump_light_probe(probe, "./bin/probes/");

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
        clear(&render_target_view, clear_color);  
        clear(&depth_buffer_view, 0xFFFFFFFF);

        Renderer::ViewPort viewport = {
            .x = 0,
            .y = 0,
            .width = width,
            .height = height,
        };

        y_rotation += delta_time;
        auto view_mat = glm::identity<glm::mat4>();
        view_mat = glm::rotate(view_mat, y_rotation, glm::vec3(0.f, -1.f, 0.f));
        view_mat = glm::translate(view_mat, camera_pos);
        auto proj_mat = glm::perspective(glm::radians(90.0f), static_cast<float>(width) / height, 0.1f, 100.f);

        for (auto& mesh : scene.meshes) {
            // remove glasses
            bool is_transparant = glm::length2(mesh.material.transmittance) < 0.99f;
            if (is_transparant) continue;

            draw_new(
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
                    .world_transform = glm::identity<glm::mat4>(),
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