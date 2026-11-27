// EXPECT: error [[stencil]] takes no argument
#include <metal_stdlib>
using namespace metal;
struct Out { uint s [[stencil(0)]]; };
fragment Out f(float4 p [[position]]) { Out o; o.s = 1u; return o; }
