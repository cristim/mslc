// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// Unsigned template parameters, u literals and a uint constant inside constant expressions: an unsigned operand
// makes the operation unsigned ([expr.arith.conv]), and with no negative value or wrap its result is the signed
// one. Thread i writes i + 400 + 7000.
#include <metal_stdlib>
using namespace metal;
template <unsigned int count, unsigned int start, typename T> inline T field(T x) {
  constexpr unsigned int mask = (1u << count) - 1u;
  return (x >> start) & mask;
}
constant uint K = 3u;
constant int arr[K * 2u + 1u] = {};
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  float a[K + 1u];
  a[0] = 1;
  out[i] = float(field<4, 4>(uint(i * 17u))) + float(sizeof(a) / sizeof(a[0])) * 100.0f + float(sizeof(arr) / 4) * 1000.0f;
}
