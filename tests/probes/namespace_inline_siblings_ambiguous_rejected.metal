// EXPECT: error reference to "A::v" is ambiguous
// Two inline namespaces of A both declare v.
#include <metal_stdlib>
using namespace metal;
namespace A { inline namespace I { constant float v = 1.0; } inline namespace J { constant float v = 2.0; } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::v; }
