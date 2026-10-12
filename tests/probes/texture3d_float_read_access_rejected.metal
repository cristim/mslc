// EXPECT: error texture3d access::read is not lowered yet
#include <metal_stdlib>
using namespace metal;
kernel void k(texture3d<float, access::read> t [[texture(0)]], device uint* o [[buffer(0)]]) {
  o[0] = t.get_width();
}
