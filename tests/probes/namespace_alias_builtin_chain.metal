// EXPECT: valid
#include <metal_stdlib>
namespace N { namespace M = ::metal; }
namespace X = N::M;
kernel void k(device float* out [[buffer(0)]]) {
    out[0] = X::clamp(5.0f, 0.0f, 1.0f);
}
