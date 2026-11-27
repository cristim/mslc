// EXPECT: error read is refused because Vulkan's OpImageFetch does not take a Cube image
// Apple has read(uint2, uint face, uint lod); OpImageFetch does not take a Cube image.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.read(uint2(0), 0);
}
