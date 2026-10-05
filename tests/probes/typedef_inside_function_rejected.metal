// EXPECT: error "typedef" inside a function is not supported
//
// A local typedef is legal; mslc keeps typedefs at file scope.
#include <metal_stdlib>
using namespace metal;
kernel void typedef_inside_function_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ typedef uint u_t; out[i] = i; }
