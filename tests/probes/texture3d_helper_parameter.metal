// EXPECT: valid
// DISASM: OpImageSampleExplicitLod %v4float
// DISASM: OpImageQuerySizeLod %v3uint
//
// A texture3d passes to a helper like a texture2d does; a vertex function
// samples at an explicit lod.
#include <metal_stdlib>
using namespace metal;
float4 look(texture3d<float> t, sampler s, float3 c) { return t.sample(s, c) + float(t.get_depth()); }
vertex float4 v(texture3d<float> t [[texture(1)]], sampler s [[sampler(0)]]) { return look(t, s, float3(0.5)); }
