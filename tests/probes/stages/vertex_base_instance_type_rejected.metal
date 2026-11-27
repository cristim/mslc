// EXPECT: error is [[base_instance]], which has to be a uint or ushort scalar
// Apple's compiler reports "type 'int' is not valid for attribute 'base_instance'", and the
// same for float and for a vector (metal 32023.883, macOS 26).
#include <metal_stdlib>
using namespace metal;
vertex float4 vertex_base_instance_type_rejected(int bi [[base_instance]]) { return float4(float(bi)); }
