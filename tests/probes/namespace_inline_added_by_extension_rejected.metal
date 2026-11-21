// EXPECT: error a later definition cannot make it inline
// The first definition decides whether a namespace is inline; Apple rejects adding it later.
#include <metal_stdlib>
using namespace metal;
namespace A { namespace I { constant float v = 1.0; } }
namespace A { inline namespace I { constant float w = 2.0; } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::w; }
