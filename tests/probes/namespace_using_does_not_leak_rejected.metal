// EXPECT: error function "f" is not a builtin mslc recognises
#include <metal_stdlib>
using namespace metal;
namespace N { float f(float x) { return x; } }
namespace M { using namespace N; float g(float x) { return f(x); } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = M::g(1.0) + f(1.0); }
