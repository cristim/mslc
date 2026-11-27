// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// The conditional operator in integer constant expressions: array lengths, enumerators and a constexpr local
// in a template instance, over a trait. Only the chosen value is evaluated ([expr.cond]), so 1 / 0 in the
// other is no error. Thread i writes 5 + 90 + 800 + 7000.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Bits { static constant int n = is_unsigned<T>::value ? 32 : 31; };
constant int K = 3;
enum E { A = K > 2 ? 5 : 1 / 0, B = K < 2 ? 7 : 9 };
template <typename T> inline float f(float x) { constexpr int bits = is_unsigned<T>::value ? 8 : 7; float a[bits]; return x * float(sizeof(a) / 4); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  float a[(K > 1 ? 4 : 2) + (A == 5 ? 1 : 0)];
  out[i] = float(sizeof(a) / 4) + float(B) * 10.0f + f<uint>(1.0f) * 100.0f + f<int>(1.0f) * 1000.0f;
}
