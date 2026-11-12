// EXPECT: valid
#include <metal_stdlib>
namespace M = ::metal;
kernel void k(device float* out [[buffer(0)]]) {
    out[0] = M::clamp(5.0f, 0.0f, 1.0f);
}
