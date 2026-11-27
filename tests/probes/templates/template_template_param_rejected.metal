// EXPECT: error a template template parameter is not lowered
#include <metal_stdlib>
using namespace metal;
template <template <typename> class C, typename T> T f(T x) { return x; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
