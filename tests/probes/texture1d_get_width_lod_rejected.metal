// EXPECT: error the lod of get_width on a texture1d has to be the literal 0
// Apple takes any constant expression equal to 0 here; mslc takes the literal 0 only (a known false rejection for 1-1, short(0) or a const int).
#include <metal_stdlib>
using namespace metal;
kernel void k(texture1d<float> t [[texture(0)]], device uint* o [[buffer(0)]]) { o[0] = t.get_width(1); }
