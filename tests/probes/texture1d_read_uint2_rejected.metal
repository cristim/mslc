// EXPECT: error the coordinate of read has to be a uint or a ushort
// Apple: read(uint2) is a no-match on a texture1d.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture1d<float> t [[texture(0)]], device float4* o [[buffer(0)]]) { o[0] = t.read(uint2(1, 0)); }
