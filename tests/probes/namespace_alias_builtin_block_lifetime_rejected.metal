// EXPECT: error "M" in "M::clamp" is not a namespace
#include <metal_stdlib>
kernel void k(device float* out [[buffer(0)]]) {
    { namespace M = ::metal;
      out[0] = M::clamp(5.0f, 0.0f, 1.0f); }
    out[1] = M::clamp(5.0f, 0.0f, 1.0f);
}
