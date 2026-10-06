// EXPECT: valid
// DISASM: OpCapability Shader
//
// Apple accepts <metal_geometric>; its declarations are in <metal_stdlib>, which mslc has built in.
#import <metal_geometric>
using namespace metal;
kernel void metal_geometric_import_accepted(device float4* out [[buffer(0)]], texture2d<float> t [[texture(0)]], uint i [[thread_position_in_grid]])
{ out[i] = float4(sqrt(abs(1.0f))); }
