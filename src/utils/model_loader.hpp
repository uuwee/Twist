#pragma once

#include "tinygltf/tiny_gltf.h"
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

    void search_node_tree(tinygltf::Model* model, int node_idx){
        const tinygltf::Node& node = model->nodes[node_idx];
        std::cout << "\t" << node.name << ", mesh=" << node.mesh << std::endl;

        if (-1 < node.mesh && node.mesh < model->meshes.size()){
            const tinygltf::Mesh& mesh = model->meshes[node.mesh];
            std::cout << "\t\t" << mesh.name << std::endl;
            
            for (const auto& primitive : mesh.primitives){
                std::cout << "\t\t\t" << model->materials[primitive.material].name << std::endl;
            }
        }

        for (const int child_idx : model->nodes[node_idx].children){
            search_node_tree(model, child_idx);
        }
    }

    Scene* load_scene(const std::filesystem::path& path){
        Scene* result = new Scene;

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;       
        bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.string());

        for (const auto& scene : model.scenes){
            std::cout << scene.name << std::endl;
            for (const int node_idx : scene.nodes){
                search_node_tree(&model, node_idx);
            }
        }

        // auto scene = model.scenes[0];
        // auto node_idx = scene.nodes[0];

        // auto node = model.nodes[node_idx];
        // auto mesh = model.meshes[node.mesh];
        // auto position_idx = mesh.primitives[0].attributes["POSITION"];
        // auto indices_idx = mesh.primitives[0].indices;
        // auto buffer_views = model.bufferViews;

        return result;
    }
}