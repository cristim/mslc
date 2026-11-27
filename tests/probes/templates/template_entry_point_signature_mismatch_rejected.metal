// EXPECT: error does not match any entry-point template
#include <metal_stdlib>
using namespace metal;
template <typename T> kernel void k(device T* out [[buffer(0)]]) { out[0] = T(1); }
template [[host_name("kf")]] kernel void k<float>(device int*);
