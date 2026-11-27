// EXPECT: error has an attribute, [[buffer]], that its type
// QUIRK (helper parameter attributes): [[buffer(n)]] needs a pointer or reference to device or constant memory on
// a helper too; Apple's compiler reports "invalid address space qualification for buffer pointee type".
#include <metal_stdlib>
using namespace metal;
float f(thread float& r [[buffer(0)]]) { return r; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { float x = 1.0f; out[i] = f(x); }
