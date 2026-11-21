// EXPECT: valid
// DISASM-MATCH: OpStore %[0-9]+ %float_1 Aligned
// A::I::v names I's v even though A declares a v of its own.
#include <metal_stdlib>
namespace A { inline namespace I { constant float v = 1.0; } }
namespace A { constant float v = 2.0; }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::I::v; }
