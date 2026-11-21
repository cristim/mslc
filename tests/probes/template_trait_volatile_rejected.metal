// EXPECT: error the trait "is_same" of a volatile type is not lowered
// Refs #341: Apple says is_same_v<volatile int, int> is false; mslc drops the qualifier from a
// type, so the trait is refused instead of answering for the type without it.
#include <metal_stdlib>
using namespace metal;
kernel void k(device int* out [[buffer(0)]]) { out[0] = is_same_v<volatile int, int> ? 1 : 0; }
