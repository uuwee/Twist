#pragma once

// #include "tinygltf/tiny_gltf.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"
#include "renderer/renderer.hpp" //TODO: Remove dependecy on renderer by creating Model.hpp or something

#include <string>
#include <filesystem>

namespace ModelLoader{

    struct Model {
        Renderer::Mesh mesh;
        Renderer::Texture<Renderer::R8G8B8A8_U> texture;
    };

    struct Scene {
        std::vector<Model> models;
    };

    // void search_node_tree(tinygltf::Model* model, int node_idx){
    //     const tinygltf::Node& node = model->nodes[node_idx];
    //     std::cout << "\t" << node.name << ", mesh=" << node.mesh << std::endl;

    //     if (-1 < node.mesh && node.mesh < model->meshes.size()){
    //         const tinygltf::Mesh& mesh = model->meshes[node.mesh];
    //         std::cout << "\t\t" << mesh.name << std::endl;
            
    //         for (const auto& primitive : mesh.primitives){
    //             std::cout << "\t\t\t" << model->materials[primitive.material].name << std::endl;
    //         }
    //     }

    //     for (const int child_idx : model->nodes[node_idx].children){
    //         search_node_tree(model, child_idx);
    //     }
    // }

    Scene* load_scene(const std::filesystem::path& path){
        Scene* result = new Scene;

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> mateirals;
        std::string warn, err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &mateirals, &warn, &err, path.string().c_str());

        for (size_t s = 0; s < shapes.size(); s++){
            size_t index_offset = 0;
            const auto& shape = shapes[s];
            std::cout << "shape name: " << shape.name << "\n"; 
            for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++){
                size_t fv = size_t(shape.mesh.num_face_vertices[f]);
                if (fv != 3){
                    std::cerr << "Warning: Non-triangle face found in OBJ file." << std::endl;
                    continue;
                }

                // Renderer::Mesh mesh{};
                // for (size_t v = 0; v < fv; v++){
                //     tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                //     vertex.position = {
                //         attrib.vertices[3 * size_t(idx.vertex_index) + 0],
                //         attrib.vertices[3 * size_t(idx.vertex_index) + 1],
                //         attrib.vertices[3 * size_t(idx.vertex_index) + 2],
                //     };
                //     // vertex.normal = {
                //     //     attrib.normals[3 * size_t(idx.normal_index) + 0],
                //     //     attrib.normals[3 * size_t(idx.normal_index) + 1],
                //     //     attrib.normals[3 * size_t(idx.normal_index) + 2],
                //     // };
                //     vertex.texcoord0 = {
                //         attrib.texcoords[2 * size_t(idx.texcoord_index) + 0],
                //         attrib.texcoords[2 * size_t(idx.texcoord_index) + 1],
                //     };
                //     mesh.vertices.push_back(vertex);
                // }
                // index_offset += fv;

                // mesh.indices.resize(fv);
                // for (size_t v = 0; v < fv; v++){
                //     mesh.indices[v] = index_offset - fv + v;
                // }
                
                // result->models.push_back({mesh, {}});
            }
        }

        return result;
    }
}