// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// Chained and nested assignments: each one's value is its left operand once stored to, converted to that
// operand's type, as in C++ (checked against host clang: 8020800, 8020811, 8120822 for i = 0, 1, 2).
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  float4 v = float4(float(i), 1, 2, 3);
  v.g = v.b = v.r;                 // g = b = i
  int x = 0; int y;
  float r = (x += 2) + (y = x * 3); // 2 + 6
  int arr[3] = {0, 0, 0};
  uint j = i % 3;
  arr[j] = arr[(j + 1) % 3] = 5;
  int z = (x = 1.75f);             // int conversion: 1
  bool b; bool c = (b = (i > 1));
  uint u = 0; u = (u = 7) + 1;     // 8
  out[i] = v.g + v.b * 10.0f + r * 100.0f + float(arr[0] + arr[1] + arr[2]) * 1000.0f + float(z) * 10000.0f + (c ? 100000.0f : 0.0f) + float(u) * 1000000.0f;
}
