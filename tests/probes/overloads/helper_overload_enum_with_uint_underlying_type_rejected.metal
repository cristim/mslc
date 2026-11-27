// EXPECT: error an enum whose underlying type is not int
// A value above INT_MAX makes the enum's type unsigned, so it promotes to uint, not int (Apple
// picks f(uint)); mslc does not track that, so the choice is refused.
#include <metal_stdlib>
using namespace metal;
enum Big { bg = 0x80000000u };
float f(int x) { return 1.0f; }
float f(uint x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(bg); }
