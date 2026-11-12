// EXPECT: valid
// DISASM-NOT: OpReturnValue %float_7
#include <metal_stdlib>
namespace N {
    namespace metal { float clamp(float a, float b, float c) { return 7.0f; } }
    namespace M = ::metal;
}
kernel void k(device float* out [[buffer(0)]]) { out[0] = N::M::clamp(5.0f, 0.0f, 1.0f); }
