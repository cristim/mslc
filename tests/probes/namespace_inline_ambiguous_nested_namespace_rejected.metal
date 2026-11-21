// EXPECT: error reference to the namespace "C" in "A" is ambiguous
// A::C and A::I::C (inline) are two namespaces of A's own set (declared in this order, Apple
// reports reference to 'C' is ambiguous).
#include <metal_stdlib>
using namespace metal;
namespace A { namespace C { constant float v = 1; } inline namespace I { namespace C { constant float w = 8; } } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::C::v; }
