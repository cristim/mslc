// EXPECT: error a reference to texture2d<float> is not valid
// Apple: refused, a texture parameter of an entry point is taken by value (black-box, xcrun metal, macOS 26).
#include <metal_stdlib>
using namespace metal;
fragment float4 f(thread texture2d<float>& t [[texture(0)]], sampler s [[sampler(0)]]) { return t.sample(s, float2(0.5)); }
