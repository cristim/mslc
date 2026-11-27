// EXPECT: error is [[base_vertex]], which has to be a uint or ushort scalar
// Apple's compiler reports "type 'uint2' ... is not valid for attribute 'base_vertex'"
// (metal 32023.883, macOS 26).
#include <metal_stdlib>
using namespace metal;
vertex float4 vertex_base_vertex_vector_rejected(uint2 bv [[base_vertex]]) { return float4(float(bv.x)); }
