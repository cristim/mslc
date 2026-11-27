// EXPECT: valid
// DISASM: OpFunctionCall
// DISASM-NOT: Sqrt
// namespace M = metal; inside N names N's own namespace metal: M::sqrt is its helper.
#include <metal_stdlib>
namespace N { namespace metal { float sqrt(float a) { return 9.0; } } namespace M = metal; float f() { return M::sqrt(4.0f); } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = N::f(); }
