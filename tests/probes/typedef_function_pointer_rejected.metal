// EXPECT: error a typedef of a function pointer is not supported
//
// Apple accepts the declaration; mslc has no function types.
#include <metal_stdlib>
using namespace metal;
typedef void (*callback)(int);
kernel void typedef_function_pointer_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
