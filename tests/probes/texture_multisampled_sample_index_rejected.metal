// EXPECT: error the sample index of read has to be an integer
// Apple accepts a float sample index (converted); mslc refuses instead of guessing.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d_ms<float> a [[texture(0)]], device float4* o [[buffer(0)]]) { o[0] = a.read(uint2(0), 1.5); }
