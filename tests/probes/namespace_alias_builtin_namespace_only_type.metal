// EXPECT: valid
#include <metal_stdlib>
namespace E { enum M { x = 2 }; }
kernel void k(device float* out [[buffer(0)]]) {
    namespace M = ::metal;
    using E::M;
    namespace X = M;
    out[0] = X::clamp(5.0f, 0.0f, 1.0f);
}
