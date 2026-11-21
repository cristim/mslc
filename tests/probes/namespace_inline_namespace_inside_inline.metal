// EXPECT: valid
// DISASM: OpStore
// A::C::v finds the namespace C of the inline namespace I.
#include <metal_stdlib>
using namespace metal;
namespace A { inline namespace I { namespace C { constant float v = 8; } } } kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::C::v; }
