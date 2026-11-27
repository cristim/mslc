// EXPECT: error reference to "A::v" is ambiguous
// A::v is A's own v and the inline namespace's: [namespace.qual] makes both A's, so Apple
// reports it ambiguous.
#include <metal_stdlib>
using namespace metal;
namespace A { inline namespace I { constant float v = 1.0; } constant float v = 2.0; }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::v; }
