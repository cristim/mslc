// EXPECT: valid
// Beside a by-value overload, an rvalue cannot bind to the mutable reference: f(1.0f) is f(float).
#include <metal_stdlib>
using namespace metal;
float f(thread float &x) { return 1.0f; }
float f(float x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
