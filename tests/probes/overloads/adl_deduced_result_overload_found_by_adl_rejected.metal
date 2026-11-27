// EXPECT: error argument-dependent lookup for "get" would search its namespace
// The inner call has a struct argument, so its own argument-dependent lookup finds M::pick, which returns a
// struct and wins: C++ then calls M::get (300). The result is not known to be a scalar, so the call is refused.
#include <metal_stdlib>
using namespace metal;
namespace M { struct S { float v; }; inline S pick(float x, S s) { return S{x + s.v}; } inline float get(S s) { return s.v * 100.0f; } }
template <typename T> static inline T pick(T x, M::S s) { return x; }
template <typename T> inline float get(T x) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { M::S s{2.0f}; out[i] = get(pick(1.0f, s)); }
