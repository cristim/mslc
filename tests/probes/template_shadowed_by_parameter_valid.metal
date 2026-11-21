// EXPECT: valid
// RUN: grid=1 local=1
// Refs #341: a parameter named like a template is the parameter, and "f < 3" is a
// comparison, not explicit template arguments.
#include <metal_stdlib>
using namespace metal;
template <typename T> T f(T x) { return x; }
kernel void k(device float* o [[buffer(0)]], uint f [[thread_position_in_grid]])
{ o[0] = f < 3 ? 1.0f : 0.0f; }
