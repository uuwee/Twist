#ifndef MAC_RASTERIZER_INCLUDED
#define MAC_RASTERIZER_INCLUDED

#include <stdint.h> // uint32_t

// ADD THIS FOR EVERY SINGLE PROCEDURE DECL IN HEADER
#ifndef MAC_API
#ifdef __cplusplus
#define MAC_API extern "C"
#else
#define MAC_API
#endif
#endif

// data types
#ifdef __cplusplus
extern "C"{
#endif


#ifdef __cplusplus
}
#endif

// procedures
MAC_API void MAC_greet();

#endif