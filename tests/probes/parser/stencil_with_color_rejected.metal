// EXPECT: error has an incompatible stencil attribute combination
#include <metal_stdlib>
using namespace metal;
struct Out { uint s [[stencil, color(0)]]; };
fragment Out f(float4 p [[position]]) { Out o; o.s = 1u; return o; }
