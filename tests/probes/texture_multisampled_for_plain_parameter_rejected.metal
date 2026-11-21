// EXPECT: error which is not a texture2d<float>
// Apple: no matching function for call to 'plain' (a multisampled texture is not a texture2d).
#include <metal_stdlib>
using namespace metal;
float4 plain(texture2d<float> t) { return float4(t.get_width()); }
fragment float4 hit(texture2d_ms<float> m [[texture(0)]]) { return plain(m); }
