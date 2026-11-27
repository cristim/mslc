// EXPECT: error is [[base_instance]], which has to be a uint or ushort scalar
// Apple's compiler reports "type 'metal::atomic_uint' ... is not valid for attribute" (metal
// 32023.883, macOS 26).
#include <metal_stdlib>
using namespace metal;
vertex float4 vertex_base_instance_atomic_rejected(atomic_uint bi [[base_instance]]) { return float4(1.0f); }
