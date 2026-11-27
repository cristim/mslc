// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// The argument of an unqualified call is a call to a template that returns T deduced from a ushort or int:
// the result is a scalar, which brings no namespace to argument-dependent lookup, so the call is lowered
// without its type being known first. On an RTX 3060 thread 1 writes 1/31 + 10 * 2/63 + 1000 = 1000.3497.
#include <metal_stdlib>
using namespace metal;
namespace N {
template <unsigned int count, unsigned int start, typename T> static inline T shifted(T input) { return (input >> start) & ((1 << count) - 1); }
template <unsigned int bits, typename T> static inline float norm(T input) { return float(input) / float((1 << bits) - 1); }
inline float norm(int input) { return float(input) * 1000.0f; }
}
using namespace N;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  ushort src = ushort(i * 0x0841u);
  out[i] = norm<5>(shifted<5, 11>(src)) + norm<6>(shifted<6, 5>(src)) * 10.0f + norm(shifted<4, 0>(int(i)));
}
