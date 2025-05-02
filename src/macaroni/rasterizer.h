#ifndef MAC_RASTERIZER_INCLUDED
#define MAC_RASTERIZER_INCLUDED

#include <stdint.h> // uint32_t

// ADD THIS FOR EVERY SINGLE PROCEDURE DECL IN HEADER
#ifndef C_API
#ifdef __cplusplus
#define C_API extern "C"
#else
#define C_API
#endif
#endif

// data types
#ifdef __cplusplus
	namespace MAC {
		template<typename T>
		struct Image {
			T* data;
			std::uint32_t width, height;
			inline T& operator[](std::uint32_t x, std::uint32_t y){
				if (0 <= x && x < width && 0 <= y && y < height){
					return data[y * width + x];
				}
				return 0;
			}
			inline T operator[](std::uint32_t x, std::uint32_t y) const{
				if (0 <= x && x < width && 0 <= y && y < height){
					return data[y * width + x];
				}
				return 0;
			}
		};
	}
	#define MAC_Image(T) MAC::Image<T>
	typedef struct {
		void* data;
		std::uint32_t width, height;
	} MAC_Image_RAW;
#else
	#define MAC_Image(T) struct { T* data; uint32_t width, height;}
	typedef MAC_Image(void) MAC_Image_RAW;
#endif	

// procedures
C_API void MAC_greet();

#endif