// EXPECT: error the lod of read on a texture1d has to be the literal 0
// Apple: read(uint, 1) and a variable lod are no-match; only the constant zero is accepted.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture1d<float> t [[texture(0)]], device float4* o [[buffer(0)]]) { o[0] = t.read(1u, 1); }
