// EXPECT: valid
// A type parameter deduced as a texture type declares a local copy of the texture, as texture2d<float> does.
#include <metal_stdlib>
using namespace metal;
template <typename Tex> static float g(Tex t) { Tex copy = t; return copy.read(uint2(0)).x; }
fragment float4 f(texture2d<float> a [[texture(0)]]) { return float4(g(a)); }
