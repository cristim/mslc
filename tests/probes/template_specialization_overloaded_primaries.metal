// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// Of two primary templates of one name, a specialization specializes the one its declaration can be
// deduced against: "float g(uint)" cannot give <unsigned Bits, typename T> its Bits, so it is <typename T>'s.
#include <metal_stdlib>
using namespace metal;
template <unsigned int Bits, typename T> static inline float g(T x) { return float(x) / float((1u << Bits) - 1u); }
template <typename T> static inline float g(T x) { return g<sizeof(T) * 8, T>(x); }
template <> inline float g(uint x) { return float(x) * (1.0f / 0xffffffff); }
kernel void k(device const uint* in [[buffer(0)]], device float* out [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = g(in[i]) + g(uchar(in[i])) + g<4>(ushort(in[i] & 15u)); }
