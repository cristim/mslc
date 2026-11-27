// EXPECT: error write ".template f<...>(...)"
// Refs #386: a call with a dependent argument is dependent through lookup; Apple rejects it bare.
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
B mk(float v) { return B{v}; }
template <typename T> float g(T x) { return mk(x).f<float>(x); }
kernel void k(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { out[i] = g(in[i]); }
