// EXPECT: error the trait "is_same" depends on a type that is not deduced yet
// Refs #341: a constant's default that reads a type parameter is evaluated before that parameter
// is deduced, so it is refused rather than evaluated for a placeholder.
#include <metal_stdlib>
using namespace metal;
template <typename T, bool B = is_same<T, float>::value> float f(T x) { return B ? 1.0f : 2.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
