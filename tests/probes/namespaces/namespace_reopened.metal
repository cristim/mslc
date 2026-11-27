// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
namespace N { constant float a = 1.0; }
namespace N { constant float b = a + 1.0; float f() { return a + b; } }
namespace N { float g() { return f() + a; } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = N::g() + N::a + N::b; }
