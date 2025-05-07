#pragma once

// #include "tinygltf/tiny_gltf.h"
#include "rapidobj/rapidobj.hpp"
#include "renderer/renderer.hpp" //TODO: Remove dependecy on renderer by creating Model.hpp or something

#include <string>
#include <filesystem>
#include <list>

namespace ModelLoader{
    struct Mesh{
        std::vector<Renderer::Vertex> vertices;
        std::vector<std::uint32_t> indices;
        Renderer::Material material;
    };

    struct Scene{
        std::vector<Mesh> meshes;
        std::list<Renderer::Texture<Renderer::R8G8B8A8_U>> textures;
    };

    Renderer::Texture<Renderer::R8G8B8A8_U>* load_texture_to_scene(Scene* scene, const std::filesystem::path& path){
        Renderer::Image<Renderer::R8G8B8A8_U> image = Renderer::load_image(path);
        scene->textures.push_back(Renderer::Texture<Renderer::R8G8B8A8_U>{});
        auto& new_texture = scene->textures.back();
        new_texture.mipmaps.push_back(image);
        Renderer::generate_mipmaps(&new_texture);
        return &new_texture;
    }

    void load_scene(Scene* scene, std::filesystem::path const& path) {
        scene->meshes.clear();
        scene->meshes.shrink_to_fit();

        rapidobj::Result result = rapidobj::ParseFile(path.string());
        if (result.error){
            std::cerr << "Error parsing OBJ file: " << result.error.code.message() << "\n";
            return;
        }

        bool success = rapidobj::Triangulate(result);
        if (!success) {
            std::cerr << "Error triangulating OBJ file: " << result.error.code.message() << "\n";
            return;
        }

        scene->meshes.resize(result.materials.size());
        for (std::size_t i = 0; i < result.materials.size(); i++){
            Mesh& mesh = scene->meshes[i];
            rapidobj::Material& obj_mat = result.materials[i];
            mesh = Mesh{
                .vertices = {},
                .indices = {},
                .material = Renderer::Material{
                    .name = obj_mat.name,
                    .ambient = glm::vec3(obj_mat.ambient.at(0), obj_mat.ambient.at(1), obj_mat.ambient.at(2)),
                    .diffuse = glm::vec3(obj_mat.diffuse.at(0), obj_mat.diffuse.at(1), obj_mat.diffuse.at(2)),
                    .specular = glm::vec3(obj_mat.specular.at(0), obj_mat.specular.at(1), obj_mat.specular.at(2)),
                    .transmittance = glm::vec3(obj_mat.transmittance.at(0), obj_mat.transmittance.at(1), obj_mat.transmittance.at(2)),
                    .emission = glm::vec3(obj_mat.emission.at(0), obj_mat.emission.at(1), obj_mat.emission.at(2)),
                    .diffuse_tex = nullptr,
                    .specular_tex = nullptr,
                },
            };
            if (obj_mat.diffuse_texname != ""){
                const std::filesystem::path tex_path = path.parent_path() / obj_mat.diffuse_texname;
                Renderer::Texture<Renderer::R8G8B8A8_U>* tex = load_texture_to_scene(scene, tex_path);
                mesh.material.diffuse_tex = tex;
            }
        }

        for (const rapidobj::Shape& shape : result.shapes) {
            const rapidobj::Mesh& mesh = shape.mesh;

            const size_t num_faces = mesh.num_face_vertices.size();
            for (size_t face_idx = 0; face_idx < num_faces; face_idx++){
                const size_t mat_id = mesh.material_ids[face_idx];
                
                const std::uint32_t num_face_vertices = static_cast<std::uint32_t>(scene->meshes[mat_id].indices.size());
                scene->meshes[mat_id].indices.push_back(num_face_vertices + 0);
                scene->meshes[mat_id].indices.push_back(num_face_vertices + 1);
                scene->meshes[mat_id].indices.push_back(num_face_vertices + 2);

                scene->meshes[mat_id].vertices.push_back(
                    Renderer::Vertex{
                        .texcoord0 = 
                            glm::vec2{
                                result.attributes.texcoords[mesh.indices[face_idx * 3 + 0].texcoord_index * 2 + 0],
                                result.attributes.texcoords[mesh.indices[face_idx * 3 + 0].texcoord_index * 2 + 1]
                            },   
                        .world_position = glm::vec4{
                            result.attributes.positions[mesh.indices[face_idx * 3 + 0].position_index * 3 + 0],
                            result.attributes.positions[mesh.indices[face_idx * 3 + 0].position_index * 3 + 1],
                            result.attributes.positions[mesh.indices[face_idx * 3 + 0].position_index * 3 + 2],
                            1.f
                        }, 
                    }
                );
                scene->meshes[mat_id].vertices.push_back(
                    Renderer::Vertex{
                        .texcoord0 = 
                            glm::vec2{
                                result.attributes.texcoords[mesh.indices[face_idx * 3 + 1].texcoord_index * 2 + 0],
                                result.attributes.texcoords[mesh.indices[face_idx * 3 + 1].texcoord_index * 2 + 1]
                            },   
                        .world_position = glm::vec4{
                            result.attributes.positions[mesh.indices[face_idx * 3 + 1].position_index * 3 + 0],
                            result.attributes.positions[mesh.indices[face_idx * 3 + 1].position_index * 3 + 1],
                            result.attributes.positions[mesh.indices[face_idx * 3 + 1].position_index * 3 + 2],
                            1.f
                        }, 
                    }
                );
                scene->meshes[mat_id].vertices.push_back(
                    Renderer::Vertex{
                        .texcoord0 = 
                            glm::vec2{
                                result.attributes.texcoords[mesh.indices[face_idx * 3 + 2].texcoord_index * 2 + 0],
                                result.attributes.texcoords[mesh.indices[face_idx * 3 + 2].texcoord_index * 2 + 1]
                            },   
                        .world_position = glm::vec4{
                            result.attributes.positions[mesh.indices[face_idx * 3 + 2].position_index * 3 + 0],
                            result.attributes.positions[mesh.indices[face_idx * 3 + 2].position_index * 3 + 1],
                            result.attributes.positions[mesh.indices[face_idx * 3 + 2].position_index * 3 + 2],
                            1.f
                        }, 
                    }
                );
            }
        }
    }
}