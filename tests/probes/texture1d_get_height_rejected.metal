// EXPECT: error a texture1d has no get_height
// Apple: no member named get_height in texture1d.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture1d<float> t [[texture(0)]], device uint* o [[buffer(0)]]) { o[0] = t.get_height(); }
