// EXPECT: error an explicit template instantiation of an entry point needs [[host_name("...")]]
// Refs #341: mslc requires [[host_name("...")]] on an explicit instantiation of an entry point.
#include <metal_stdlib>
using namespace metal;
template <typename T> kernel void k(device T* out [[buffer(0)]]) { out[0] = T(1); }
template kernel void k<float>(device float*);
