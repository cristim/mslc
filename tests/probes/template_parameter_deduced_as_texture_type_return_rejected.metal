// EXPECT: error "Tex" is the type texture2d<float>, a texture or sampler type, which mslc takes as a parameter only
// A type parameter deduced as a texture type cannot be the return type, as texture2d<float> cannot.
#include <metal_stdlib>
using namespace metal;
template <typename Tex> static Tex g(Tex t) { return t; }
fragment float4 f(texture2d<float> a [[texture(0)]]) { g(a); return float4(0); }
