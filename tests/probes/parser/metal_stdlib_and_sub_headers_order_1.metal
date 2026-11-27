// EXPECT: valid
// DISASM: OpCapability Shader
//
// A sub-header and <metal_stdlib> are one built-in header: neither order redefines anything.
#include <metal_stdlib>
#include <metal_math>
#import <metal_texture>
#include <metal_stdlib>
using namespace metal;
kernel void metal_stdlib_and_sub_headers_order_1(device float4* out [[buffer(0)]], texture2d<float> t [[texture(0)]], uint i [[thread_position_in_grid]])
{ out[i] = float4(sqrt(abs(1.0f))); }
