// EXPECT: error the coordinate of sample has to be a float2 or a number
// Apple: no matching member function for call to 'sample'; the layer is a
// separate argument, not folded into a float3 coordinate.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d_array<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float3(0.5), 0u);
}
