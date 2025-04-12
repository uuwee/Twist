#include <iostream>
#include <fstream>
#include <string>
#include <array>

int main() {
    std::cout << "hello, world!" << std::endl;

    using color = std::array<float, 3>;

    constexpr int width = 600, height = 400;
    color* image = new color[width * height];
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            int idx = i * width + j;
            image[idx][0] = static_cast<float>(j) / width; // Red
            image[idx][1] = static_cast<float>(i) / height; // Green
            image[idx][2] = 0.5; // Blue
        }
    }

    constexpr float max_value = 255.0;
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

    delete[] image;

    return 0;
}