// EXPECT: error no matching function for call to "g"
// texture2d<T> deduces nothing from a texture3d.
#include <metal_stdlib>
using namespace metal;
template <typename T> float g(texture2d<T> a) { return 1.0f; }
fragment float4 f(texture3d<float> t [[texture(0)]]) { return float4(g(t)); }
