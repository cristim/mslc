// EXPECT: valid
// DISASM: NClamp %float_5 %float_0 %float_1
// "namespace M = ::metal;" names the library, so M::clamp is metal::clamp. Apple accepts it;
// it was rejected as "not a namespace the file declares".
#include <metal_stdlib>
namespace M = ::metal;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = M::clamp(5.0f, 0.0f, 1.0f); }
