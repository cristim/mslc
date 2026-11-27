// EXPECT: valid
// texture.fence() lowers to OpMemoryBarrier with Device scope and AcquireRelease | ImageMemory semantics.
// DISASM-MATCH: OpMemoryBarrier %uint_1 %uint_2056
#include <metal_stdlib>
using namespace metal;
void flushTex(texture2d<float, access::read_write> t) { return t.fence(); }
kernel void k(texture2d<float, access::read_write> rw [[texture(0)]],
    texture2d<float, access::write> w [[texture(1)]],
    texture2d<float, access::read> r [[texture(2)]],
    texture2d<float> s [[texture(3)]],
    texture_buffer<float, access::read_write> tb [[texture(4)]],
    texture1d<float, access::write> t1d [[texture(5)]],
    texturecube<float, access::write> tc [[texture(6)]],
    device float4* o [[buffer(0)]]) {
  rw.write(float4(1), uint2(0));
  flushTex(rw);
  w.fence();
  r.fence();
  s.fence();
  tb.fence();
  t1d.fence();
  (void)tc.fence();
  o[0] = rw.read(uint2(0));
}
vertex float4 v(texture2d<float, access::read_write> rw [[texture(0)]], uint vid [[vertex_id]]) {
  rw.fence();
  return rw.read(uint2(vid, 0));
}
fragment float4 f(texture2d<float, access::read_write> rw [[texture(0)]], float4 p [[position]]) {
  rw.fence();
  return rw.read(uint2(p.xy));
}
