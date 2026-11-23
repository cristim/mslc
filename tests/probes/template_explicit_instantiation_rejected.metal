// EXPECT: error an explicit template instantiation of an entry point is not lowered
// Refs #341: Apple accepts this and emits a "kf" entry point; that needs host_name entry points.
#include <metal_stdlib>
using namespace metal;
template <typename T> kernel void k(device T* out [[buffer(0)]]) { out[0] = T(1); }
template [[host_name("kf")]] kernel void k<float>(device float*);
