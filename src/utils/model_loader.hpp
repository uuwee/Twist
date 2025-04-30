#pragma once

// #include "tinygltf/tiny_gltf.h"
#include "rapidobj/rapidobj.hpp"
#include "renderer/renderer.hpp" //TODO: Remove dependecy on renderer by creating Model.hpp or something

#include <string>
#include <filesystem>

namespace ModelLoader{
    struct Mesh{
        std::vector<Renderer::Vertex> vertices;
        std::vector<std::uint32_t> indices;
        rapidobj::Material material;
        std::optional<Renderer::Texture<Renderer::R8G8B8A8_U>> texture;
    };

    struct Scene{
        std::vector<Mesh> meshes;
    };

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
            scene->meshes[i].material = result.materials[i];
            std::cout << "transmittance: " << scene->meshes[i].material.name << " : " << scene->meshes[i].material.transmittance.at(0) << ", " << scene->meshes[i].material.transmittance.at(1) << ", " << scene->meshes[i].material.transmittance.at(2) << "\n";
            scene->meshes[i].vertices = {};
            scene->meshes[i].indices = {};
            if (scene->meshes[i].material.diffuse_texname != ""){
                std::cout << "load texture:" << scene->meshes[i].material.diffuse_texname << std::endl;
                auto tex = Renderer::load_image(path.parent_path() / scene->meshes[i].material.diffuse_texname);
                scene->meshes[i].texture = Renderer::Texture<Renderer::R8G8B8A8_U>();
                scene->meshes[i].texture->mipmaps.push_back( tex);
                Renderer::generate_mipmaps(&scene->meshes[i].texture.value());
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
                    {glm::vec4{
                        result.attributes.positions[mesh.indices[face_idx * 3 + 0].position_index * 3 + 0],
                        result.attributes.positions[mesh.indices[face_idx * 3 + 0].position_index * 3 + 1],
                        result.attributes.positions[mesh.indices[face_idx * 3 + 0].position_index * 3 + 2],
                        1.f
                    }, glm::vec2{
                        result.attributes.texcoords[mesh.indices[face_idx * 3 + 0].texcoord_index * 2 + 0],
                        result.attributes.texcoords[mesh.indices[face_idx * 3 + 0].texcoord_index * 2 + 1]
                    }}
                );
                scene->meshes[mat_id].vertices.push_back(
                    {glm::vec4{
                        result.attributes.positions[mesh.indices[face_idx * 3 + 1].position_index * 3 + 0],
                        result.attributes.positions[mesh.indices[face_idx * 3 + 1].position_index * 3 + 1],
                        result.attributes.positions[mesh.indices[face_idx * 3 + 1].position_index * 3 + 2],
                        1.f
                    }, glm::vec2{
                        result.attributes.texcoords[mesh.indices[face_idx * 3 + 1].texcoord_index * 2 + 0],
                        result.attributes.texcoords[mesh.indices[face_idx * 3 + 1].texcoord_index * 2 + 1]
                    }}
                );
                scene->meshes[mat_id].vertices.push_back(
                    {glm::vec4{
                        result.attributes.positions[mesh.indices[face_idx * 3 + 2].position_index * 3 + 0],
                        result.attributes.positions[mesh.indices[face_idx * 3 + 2].position_index * 3 + 1],
                        result.attributes.positions[mesh.indices[face_idx * 3 + 2].position_index * 3 + 2],
                        1.f
                    }, glm::vec2{
                        result.attributes.texcoords[mesh.indices[face_idx * 3 + 2].texcoord_index * 2 + 0],
                        result.attributes.texcoords[mesh.indices[face_idx * 3 + 2].texcoord_index * 2 + 1]
                    }}
                );
            }
        }
    }
}