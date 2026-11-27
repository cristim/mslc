// EXPECT: valid
// DISASM: OpBitcast %uint %float_1
// DISASM-NOT: OpConstant %uint 1065353216
// as_type of a constant is the constant's bits: as_type<uint>(1.0f) is not folded as the conversion to 1u.
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint* out [[buffer(0)]]) {
  constexpr uint one = as_type<uint>(1.0f);
  out[0] = one;
}
