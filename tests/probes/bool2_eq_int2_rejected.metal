// EXPECT: error implicit conversions between vector types

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]]) {
  bool2 a = bool2(out[1] != 0, out[2] != 0);
  int2 b = int2(out[3], out[4]);
  bool2 c = (a == b);
  out[0] = c.x ? 1 : 0;
}
