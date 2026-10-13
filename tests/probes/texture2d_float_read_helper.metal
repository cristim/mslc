// EXPECT: valid
// A helper takes the float read texture by value and reads the caller's own image.
// DISASM-MATCH: OpTypeImage %float 2D 0 0 0 2 Unknown
// DISASM-MATCH: OpImageRead %v4float
// DISASM-MATCH: OpImageQuerySize %v2uint
#include <metal_stdlib>
using namespace metal;
float4 fetch(texture2d<float, access::read> t, uint2 c) {
  return c.x < t.get_width() ? t.read(c) : float4(0.0);
}
kernel void hit(texture2d<float, access::read> src [[texture(0)]], device float4* out [[buffer(0)]], uint3 g [[thread_position_in_grid]]) {
  out[g.x] = fetch(src, g.xy);
}
