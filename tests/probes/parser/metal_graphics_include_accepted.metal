// EXPECT: valid
// DISASM: OpCapability Shader
//
// Apple accepts <metal_graphics>; its declarations are in <metal_stdlib>, which mslc has built in.
#include <metal_graphics>
using namespace metal;
kernel void metal_graphics_include_accepted(device float4* out [[buffer(0)]], texture2d<float> t [[texture(0)]], uint i [[thread_position_in_grid]])
{ out[i] = float4(sqrt(abs(1.0f))); }
