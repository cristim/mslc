// EXPECT: error access::read_write textures lower only read()
// Apple: no member function sample in access::read_write texture2d.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::read_write> a [[texture(0)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]]) {
  o[0] = a.sample(s, float2(0.5));
}
