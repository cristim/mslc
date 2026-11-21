// EXPECT: error which competes with the standard library
// Apple calls metal::abs(int) here, not the A::abs that argument-dependent lookup finds; the
// two compete by overload, which mslc does not model, so the call is refused.
#include <metal_stdlib>
using namespace metal;
namespace A { enum E { ea = 1 }; float abs(float x) { return 7.0f; } }
kernel void k(device float* out [[buffer(0)]]) { out[0] = float(abs(A::ea)); }
