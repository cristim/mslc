// EXPECT: error is [[base_vertex]], which has to be a uint or ushort scalar
// Apple's compiler reports "type 'uint &' ... is not valid for attribute 'base_vertex'", and
// the same for const uint& and atomic_uint (metal 32023.883, macOS 26).
#include <metal_stdlib>
using namespace metal;
vertex float4 vertex_base_vertex_reference_rejected(thread uint& bv [[base_vertex]]) { return float4(float(bv)); }
