// EXPECT: error the trait "is_floating_point" of a pointer, array, texture, sampler, atomic or address-space-qualified type is not lowered
// Refs #341: a trait of a type with an address space or a pointer is not folded.
#include <metal_stdlib>
using namespace metal;
template <typename T, typename E = typename enable_if<is_floating_point<thread T*>::value>::type> float f(T x) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
