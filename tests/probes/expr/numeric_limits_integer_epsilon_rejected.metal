// EXPECT: error numeric_limits<int>::epsilon() is zero for an integer type
// An integer type's epsilon() and infinity() are zero in C++; mslc does not lower them.
#include <metal_stdlib>
using namespace metal;
kernel void numeric_limits_integer_epsilon_rejected(device int* out [[buffer(0)]]) { out[0] = numeric_limits<int>::epsilon(); }
