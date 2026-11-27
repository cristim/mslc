// EXPECT: error no matching function for call to "g"
// texture2d<T> is access::sample, and deduces nothing from an access::read texture.
#include <metal_stdlib>
using namespace metal;
template <typename T> float g(texture2d<T> a) { return 1.0f; }
fragment float4 f(texture2d<float, access::read> t [[texture(0)]]) { return float4(g(t)); }
