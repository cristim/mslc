// EXPECT: error is declared but never defined
// ::f(int) is the exact match (Apple calls it, and the link fails); it must not drop out of the
// set and leave B::f(float) to be called instead.
#include <metal_stdlib>
using namespace metal;
float f(int x);
namespace B { float f(float x) { return 2.0f; } }
using namespace B;
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1); }
