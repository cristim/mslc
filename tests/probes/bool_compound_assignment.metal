// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// Compound assignment with a bool operand follows the binary operator: x += a promotes a, c |= b and d += 1
// convert the int result back to bool, a bool vector takes &= and ^= with a scalar widened. Threads 0..3 write
// 331010, 321111, 211120 and 201122 on an RTX 3060.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  bool a = (i & 1) != 0, b = (i & 2) != 0;
  int x = 10; x += a; x <<= b; x &= ~int(0);
  bool c = a; c |= b;
  bool d = a; d += 1;
  bool2 v = bool2(a, b); v &= bool2(true, b); v ^= true;
  uchar u = 3; u -= b;
  out[i] = float(x) + float(c) * 100.0f + float(d) * 1000.0f + float(v.x) * 10000.0f + float(v.y) * 20000.0f + float(u) * 100000.0f;
}
