#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <chrono>
#include <algorithm>
#include <execution> 

int main() {
    std::cout << "hello, world!" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    using Precision = float;
    using Color = std::array<Precision, 3>;

    constexpr int width = 600, height = 400;
    Color* image = new Color[width * height];
    int* pixel_indices = new int[width * height];
    std::iota(pixel_indices, pixel_indices + width * height, 0); 
    std::for_each(std::execution::unseq, pixel_indices, pixel_indices + width * height, [image](int idx) {
        Color* pixel = (image + idx);
        int s = idx / width; 
        int t = idx % width; 
        pixel->at(0) = static_cast<Precision>(s) / width; 
        pixel->at(1) = static_cast<Precision>(t) / height; 
        pixel->at(2) = 0.5; 
    });

    using Vertex = std::array<Precision, 6>;
    using Index = uint32_t;
    std::vector<Vertex> vertices = {
        {-0.5, 0.5, 0.0, 1.0, 0.0, 0.0}, 
        {0.5, 0.5, 0.0, 0.0, 1.0, 0.0}, 
        {0.5, -0.5, 0.0, 0.0, 0.0, 1.0}, 
        {-0.5, -0.5, 0.0, 0.0, 0.0, 0.0},
    };
    std::vector<Index> indices = {0, 1, 2, 0, 2, 3};
    std::for_each(std::execution::par_unseq, pixel_indices, pixel_indices + width * height, [image, vertices, indices](int idx) {
        Color* pixel = (image + idx);
        Precision s = static_cast<Precision>(idx / width) / static_cast<Precision>(height) - 0.5f; 
        Precision t = static_cast<Precision>(idx % width) / static_cast<Precision>(width) - 0.5f; 
        s *= 2.0f;
        t *= 2.0f;
        Color value = *pixel;
        
        for (size_t i = 0; i < indices.size(); i += 3) {
            Vertex v0 = vertices[indices[i]];
            Vertex v1 = vertices[indices[i + 1]];
            Vertex v2 = vertices[indices[i + 2]];

            bool inside = false;
            Precision alpha = ((v1[1] - v2[1]) * (s - v2[0]) + (v2[0] - v1[0]) * (t - v2[1])) / 
                              ((v1[1] - v2[1]) * (v0[0] - v2[0]) + (v2[0] - v1[0]) * (v0[1] - v2[1]));
            Precision beta = ((v2[1] - v0[1]) * (s - v2[0]) + (v0[0] - v2[0]) * (t - v2[1])) /
                                ((v1[1] - v2[1]) * (v0[0] - v2[0]) + (v2[0] - v1[0]) * (v0[1] - v2[1]));
            Precision gamma = 1.0f - alpha - beta;
            if (alpha >= 0 && beta >= 0 && gamma >= 0) {
                inside = true;
            }

            if (inside) {
                value[0] = alpha * v0[3] + beta * v1[3] + gamma * v2[3];
                value[1] = alpha * v0[4] + beta * v1[4] + gamma * v2[4];
                value[2] = alpha * v0[5] + beta * v1[5] + gamma * v2[5];

                break;
            }
        }

        *pixel = value;
    });

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    start = end;
    std::cout << "Render in: " << duration.count() << " ms" << std::endl;


    constexpr Precision max_value = 255.0;
    std::string header = "P3\n" + std::to_string(width) + " " + std::to_string(height) + "\n255\n";

    std::ofstream outFile("./bin/output.ppm");
    if (!outFile) {
        std::cerr << "Error creating output file." << std::endl;
        return 1;
    }
    outFile << header;
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            int idx = i * width + j;
            outFile << static_cast<int>(image[idx][0] / 1.0 * max_value) 
                << " " << static_cast<int>(image[idx][1] / 1.0 * max_value) 
                << " " << static_cast<int>(image[idx][2] / 1.0 * max_value) << "\n";
        }
    }
    outFile.close();

    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    std::cout << "Output ppm in: " << duration.count() << " ms" << std::endl;

    delete [] pixel_indices;
    delete[] image;

    return 0;
}