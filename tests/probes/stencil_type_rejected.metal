// EXPECT: error has [[stencil]], which has to be a scalar uint
#include <metal_stdlib>
using namespace metal;
struct Out { float4 c [[color(0)]]; int s [[stencil]]; };
fragment Out f(float4 p [[position]]) { Out o; o.c = p; o.s = 1; return o; }
