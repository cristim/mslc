// EXPECT: error a typedef that declares several names is not supported
//
// Apple accepts "typedef float a, b;"; mslc would have to read the second name as a second typedef, and says so instead.
#include <metal_stdlib>
using namespace metal;
typedef float a, b;
kernel void typedef_several_names_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
