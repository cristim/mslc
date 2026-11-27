// EXPECT: error has more than one [[stencil]] field
#include <metal_stdlib>
using namespace metal;
struct Out { uint a [[stencil]]; uint b [[stencil]]; };
fragment Out f(float4 p [[position]]) { Out o; o.a = 1u; o.b = 2u; return o; }
