// EXPECT: error numeric_limits<...>::digits is not lowered yet
// The static data members (digits, is_signed, has_infinity, ...) are valid MSL that mslc does not lower yet.
#include <metal_stdlib>
using namespace metal;
kernel void numeric_limits_member_rejected(device int* out [[buffer(0)]]) { out[0] = numeric_limits<int>::digits; }
