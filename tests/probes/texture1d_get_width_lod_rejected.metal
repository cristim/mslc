// EXPECT: error the lod of get_width on a texture1d has to be the literal 0
// Apple: get_width(1) is a no-match; only the constant zero is accepted.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture1d<float> t [[texture(0)]], device uint* o [[buffer(0)]]) { o[0] = t.get_width(1); }
