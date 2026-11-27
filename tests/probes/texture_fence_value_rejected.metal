// EXPECT: error texture fence returns void and has no value
// Apple rejects assigning or reading the void result of fence().
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::read_write> a [[texture(0)]], device int* o [[buffer(0)]]) {
  int x = a.fence();
  o[0] = x;
}
