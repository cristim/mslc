// EXPECT: error get_num_samples() is not allowed within a vertex function
// Apple's compiler refuses it outside a fragment function, "uses unsupported builtin functions"
// (metal 32023.883, macOS 26).
#include <metal_stdlib>
using namespace metal;
vertex float4 get_num_samples_vertex_rejected() { return float4(float(get_num_samples())); }
