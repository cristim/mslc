// EXPECT: error an unqualified call has an argument that is a call to a function template whose result type
// T is deduced from a struct of namespace M, which argument-dependent lookup would search.
#include <metal_stdlib>
using namespace metal;
namespace M { struct S { float v; }; inline float get(S s) { return s.v; } }
template <typename T> static inline T same(T x) { return x; }
inline float get(float x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { M::S s{1.0f}; out[i] = get(same(s)); }
