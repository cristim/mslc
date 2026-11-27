// EXPECT: error [[stencil]] is not valid on an input parameter
#include <metal_stdlib>
using namespace metal;
fragment float4 f(uint s [[stencil]]) { return float4(float(s)); }
