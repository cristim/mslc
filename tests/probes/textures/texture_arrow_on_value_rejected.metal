// EXPECT: error is called on a texture, not on a pointer to one
// Apple rejects it too.
#include <metal_stdlib>
using namespace metal;

fragment float4 f(texture2d<float> t [[texture(0)]], texture2d<float> u [[texture(1)]], sampler s [[sampler(0)]], float4 c [[position]]) { return t->sample(s, float2(0.5)); }
