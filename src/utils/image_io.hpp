#pragma once
 
namespace ImageIO{
	void dump_surface_to_ppm(const SDL_Surface& surface){
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

void dump_image_to_ppm(const Renderer::Image<Renderer::R8G8B8A8_U>& image, const std::string& filename){
    std::ofstream out_File(filename);
    if (!out_File) {
        std::cerr << "Error creating output file.: " << filename << "\n";
        return;
    }

    out_File << "P3\n" << image.width << " " << image.height << "\n255\n";
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            auto pixel = image.at(x, y);
            out_File << static_cast<int>(pixel.r) << " " << static_cast<int>(pixel.g) << " " << static_cast<int>(pixel.b) << "\n";
        }
    }
    out_File.close();
}

void dump_image_to_ppm(const Renderer::Image<std::uint32_t>& image, const std::string& filename){
    std::ofstream out_File(filename);
    if (!out_File) {
        std::cerr << "Error creating output file." << std::endl;
        return;
    }

    out_File << "P3\n" << image.width << " " << image.height << "\n" << 255 <<"\n";
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            auto pixel = static_cast<double>(image.at(x, y)) / static_cast<double>(UINT32_MAX) * 255;
            out_File << pixel << " 0 0" << "\n";
        }
    }
    out_File.close();
}

// template<typename Renderer::R8G8B8A8_U>
void dump_texture_to_ppm(const Renderer::Texture<Renderer::R8G8B8A8_U>& texture, const std::filesystem::path& directory_path){
	if (!std::filesystem::is_directory(directory_path)){
		std::cerr << "Failed to dump texture to ppm. directory:" << directory_path << " is not directory name.\n";
	}

	if (!std::filesystem::exists(directory_path)){
		std::cerr << "Failed to dump texture to ppm. directory: " << directory_path << " does not exists.\n";
	}

	for (std::uint32_t miplevel = 0; miplevel < texture.mipmaps.size(); miplevel++){
		const std::filesystem::path file_name = directory_path / ("mip_" + std::to_string(miplevel) + ".ppm");
		const Renderer::Image<Renderer::R8G8B8A8_U>& img = texture.mipmaps.at(miplevel);
		dump_image_to_ppm(img, file_name.generic_string());
	}
}
}