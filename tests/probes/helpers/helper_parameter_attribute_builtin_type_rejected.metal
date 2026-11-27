// EXPECT: error has an attribute, [[position]], that its type float is not valid for
// QUIRK (helper parameter attributes): [[position]] needs a float4 on a helper too, as Apple's compiler reports
// ("type 'float' is not valid for attribute 'position'").
#include <metal_stdlib>
using namespace metal;
float f(float x [[position]]) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(1.0f); }
