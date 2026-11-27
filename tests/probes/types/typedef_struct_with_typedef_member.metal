// EXPECT: valid
//
// A struct field of a typedef'd type.
#include <metal_stdlib>
using namespace metal;
typedef float2 uv_t;
typedef struct { uv_t uv; float w; } Outer;
kernel void typedef_struct_with_typedef_member(device Outer* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i].uv = float2(1.0, 2.0); out[i].w = 3.0; }
