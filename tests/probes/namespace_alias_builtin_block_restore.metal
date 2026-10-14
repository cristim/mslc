// EXPECT: valid
// DISASM: OpReturnValue %float_7
#include <metal_stdlib>
namespace User { float clamp(float a, float b, float c) { return 7.0f; } }
namespace M = User;
kernel void k(device float* out [[buffer(0)]]) {
    { namespace M = ::metal;
      out[0] = M::clamp(5.0f, 0.0f, 1.0f); }
    out[1] = M::clamp(5.0f, 0.0f, 1.0f);
}
