// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
constant float k = 1.0;
float f() { return 3.0; }
namespace N { constant float k = 2.0; float f() { return 4.0; } float both() { return ::k + k + ::f() + f(); } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = N::both() + ::k; }
