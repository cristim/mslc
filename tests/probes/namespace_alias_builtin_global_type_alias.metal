// EXPECT: valid
#include <metal_stdlib>
namespace User { struct metal { int x; }; }
using User::metal;
namespace X = ::metal;
kernel void k(device float* out [[buffer(0)]]) {
    out[0] = X::clamp(5.0f, 0.0f, 1.0f);
}
