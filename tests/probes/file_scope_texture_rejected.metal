// EXPECT: error a file-scope texture2d<float>
// Apple accepts no texture at program scope either; the sampler is the only resource mslc lowers there.
#include <metal_stdlib>
using namespace metal;
constant texture2d<float> T;
fragment float4 f() { return float4(0.0); }
