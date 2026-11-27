// EXPECT: error argument-dependent lookup for "get" would search its namespace
// in + 1 points to a struct of M, which brings M to argument-dependent lookup, so the deduced result is
// not taken for a scalar.
#include <metal_stdlib>
using namespace metal;
namespace M { struct S { float v; }; inline float get(device S* s) { return s->v * 100.0f; } }
template <typename T> static inline T same(T x) { return x; }
template <typename T> inline float get(T x) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]], device M::S* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { out[i] = get(same(in + 1)); }
