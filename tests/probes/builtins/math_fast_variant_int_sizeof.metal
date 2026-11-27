// EXPECT: valid
// DISASM-NOT: %ulong_2
// DISASM-NOT: %ulong_8
// Apple accepts this: a scalar int argument of a fast or precise function converts to float, so each sizeof is
// sizeof(float) (black-box, xcrun metal, macOS 26).
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint* out [[buffer(0)]]) {
    out[0] = sizeof(precise::min(1, 2));
    out[1] = sizeof(fast::clamp(5, 0, 3));
    out[2] = sizeof(precise::select(1, 2, true));
    out[3] = sizeof(fast::abs(short(1)));
}
