// EXPECT: error does not match any entry-point template
#include <metal_stdlib>
using namespace metal;
enum E { A = 1 };
template <typename T> kernel void k(device int* out [[buffer(0)]], constant T* v [[buffer(1)]]) { out[0] = int(*v); }
template [[host_name("kf")]] kernel void k(device int*, constant E*);
