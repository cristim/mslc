// EXPECT: error is called on a pointer to a texture
// Apple rejects it too: a member of a pointer is reached with ->.
#include <metal_stdlib>
using namespace metal;

fragment float4 f(texture2d<float> t [[texture(0)]], texture2d<float> u [[texture(1)]], sampler s [[sampler(0)]], float4 c [[position]]) { thread texture2d<float>* p = &t; return p.sample(s, float2(0.5)); }
