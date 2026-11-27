// EXPECT: valid
// DISASM: OpFAdd %float %float_1 %float_2
// A later namespace A { namespace I { ... } } extends the inline namespace I, whose members
// stay A's: A::w (declared by the extension) is found, and so is A::v (declared inline).
#include <metal_stdlib>
using namespace metal;
namespace A { inline namespace I { constant float v = 1.0; } }
namespace A { namespace I { constant float w = 2.0; } }
namespace A { constant float z = 3.0; }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::v + A::w + A::z + A::I::w; }
