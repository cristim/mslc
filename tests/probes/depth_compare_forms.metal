// EXPECT: valid
// sample_compare is OpImageSampleDref*Lod and returns a float; gather_compare is OpImageDrefGather. The comparison itself is the sampler's state, which a sampler parameter leaves to the host.
// DISASM-MATCH: OpImageSampleDrefImplicitLod %float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %[0-9A-Za-z_]+
// DISASM-MATCH: OpImageSampleDrefExplicitLod %float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Lod\|ConstOffset
// DISASM-MATCH: OpImageSampleDrefExplicitLod %float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Grad
// DISASM-MATCH: OpImageDrefGather %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ ConstOffset
// REFLECT-NOT: LessEqual
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(depth2d<float> a [[texture(0)]], depth2d_array<float> b [[texture(1)]], depthcube<float> c [[texture(2)]], depthcube_array<float> d [[texture(3)]], sampler s [[sampler(0)]]) {
  float x = a.sample_compare(s, float2(0.5), 0.5);
  x += a.sample_compare(s, float2(0.5), 0.5, level(1), int2(1, 0));
  x += a.sample_compare(s, float2(0.5), 0.5, gradient2d(float2(0.1), float2(0.2)));
  x += b.sample_compare(s, float2(0.5), 1u, 0.25) + c.sample_compare(s, float3(1, 0, 0), 0.5) + d.sample_compare(s, float3(1, 0, 0), 2u, 0.5);
  return a.gather_compare(s, float2(0.5), 0.5, int2(1, 0)) + c.gather_compare(s, float3(1, 0, 0), 0.5) + float4(x);
}
