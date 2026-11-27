// EXPECT: valid
// DISASM: %_ptr_Input_v4float = OpTypePointer Input %v4float
// DISASM-NOT: OpTypePointer Input %v4half
// DISASM-MATCH: OpFConvert %v4half %[0-9]+
//
// A half4 attribute is an Input of float4 that the function narrows, because
// Vulkan's shaderFloat16 does not cover the Input storage class. A buffer in
// R16G16B16A16_SFLOAT feeds a float4 Input exactly.
#include <metal_stdlib>
using namespace metal;
struct In { half4 color [[attribute(0)]]; };
struct Out { float4 p [[position]]; half4 c; };
vertex Out vertex_attribute_half_crosses_as_float(In in [[stage_in]])
{ Out o; o.p = float4(in.color); o.c = in.color; return o; }
