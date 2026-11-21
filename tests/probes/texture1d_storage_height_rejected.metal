// EXPECT: error a texture1d has no get_height
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture1d<float, access::read_write> a [[texture(0)]], device uint* o [[buffer(0)]]) {
  o[0] = a.get_height();
}
