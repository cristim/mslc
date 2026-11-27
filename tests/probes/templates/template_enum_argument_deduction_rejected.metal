// EXPECT: error which is an enum
// Refs #341: T would be the enum type, which mslc does not carry through a template.
#include <metal_stdlib>
using namespace metal;
enum E { ea = 3, eb };
template <typename T> float f(T x) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(eb); }
