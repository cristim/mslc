// EXPECT: error an attribute or specifier before "kernel" in an entry-point template is not lowered
#include <metal_stdlib>
using namespace metal;
template <typename T> [[host_name("kf")]] kernel void k(device T* out [[buffer(0)]]) { out[0] = T(1); }
