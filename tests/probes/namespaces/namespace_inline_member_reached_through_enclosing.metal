// EXPECT: valid
// DISASM: OpFunctionCall
// DISASM: OpFAdd %float %float_1 %float_1
// A::v, A::f and A::I::v all name I's members; A::f() is a call, not folded away.
#include <metal_stdlib>
using namespace metal;
namespace A { inline namespace I { constant float v = 1.0; float f() { return 2.0; } } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::v + A::I::v + A::f(); }
