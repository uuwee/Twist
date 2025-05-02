#ifndef MAC_RASTERIZER_INCLUDED
#define MAC_RASTERIZER_INCLUDED

// ADD THIS FOR EVERY SINGLE PROCEDURE DECL IN HEADER
// this will add extern "C" if this header is included
// by c++ code.
#ifndef C_API
#ifdef __cplusplus
#define C_API extern "C"
#else
#define C_API
#endif
#endif



C_API void MAC_greet();


#endif