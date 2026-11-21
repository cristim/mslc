// EXPECT: valid
// DISASM: OpFunctionCall
// An alias of metal in N names the library's float4 in a return type and a constructor.
#include <metal_stdlib>
namespace N { namespace M = ::metal; M::float4 f() { return M::float4(1.0); } } kernel void kern(device float* out [[buffer(0)]]) { out[0] = N::f().x; }
