// EXPECT: valid
// DISASM: OpReturnValue %float_7
#include <metal_stdlib>
namespace N {
    namespace metal { float clamp(float a, float b, float c) { return 7.0f; } }
    namespace M = metal;
    float g() { return M::clamp(5.0f, 0.0f, 1.0f); }
}
kernel void k(device float* out [[buffer(0)]]) { out[0] = N::g(); }
