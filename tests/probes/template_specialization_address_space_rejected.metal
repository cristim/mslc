// EXPECT: error the explicit specialization of "f" at line 8 matches no function template of that name declared above it
// A reference parameter's address space is part of the function type: thread const int& does not specialize
// a template taking constant T&.
#include <metal_stdlib>
using namespace metal;
constant int C = 5;
template <typename T> float f(constant T& x) { return 1.0f; }
template <> float f<int>(thread const int& x) { return float(x) * 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f<int>(C); }
