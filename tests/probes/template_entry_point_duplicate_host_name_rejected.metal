// EXPECT: error two entry points are named "kf"
#include <metal_stdlib>
using namespace metal;
template <typename T> kernel void k(device T* out [[buffer(0)]]) { out[0] = T(1); }
template [[host_name("kf")]] kernel void k<float>(device float*);
template [[host_name("kf")]] kernel void k<int>(device int*);
