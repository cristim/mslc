// EXPECT: error has the attribute [[flat]], which applies to a struct member only
// QUIRK (helper parameter attributes): Apple's compiler refuses an interpolation attribute on a parameter
// ("'flat' attribute only applies to non-static data members").
#include <metal_stdlib>
using namespace metal;
float f(float x [[flat]]) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(1.0f); }
