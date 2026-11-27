// EXPECT: valid
// read(uint2) is a fetch of level 0 with no sampler.
// DISASM-MATCH: OpImageFetch %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Lod %uint_0(_[0-9])?[^_0-9a-zA-Z]
// DISASM-NO-MATCH: OpSampledImage
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.read(uint2(1, 2));
}
