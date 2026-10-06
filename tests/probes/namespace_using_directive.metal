// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
namespace N { constant float k = 2.0; float f(float x) { return x * k; } struct S { float a; }; }
namespace M { using namespace N; float g(float x) { return f(x) + k; } }
using namespace N;
kernel void kern(device float* out [[buffer(0)]]) { S s; s.a = f(1.0) + k; out[0] = s.a + M::g(1.0); }
