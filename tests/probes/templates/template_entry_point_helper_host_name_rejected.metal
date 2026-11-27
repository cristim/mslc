// EXPECT: error [[host_name]] is only supported on an explicit instantiation of an entry point
#include <metal_stdlib>
using namespace metal;
template <typename T> T f(T x) { return x; }
template [[host_name("q")]] float f<float>(float);
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
