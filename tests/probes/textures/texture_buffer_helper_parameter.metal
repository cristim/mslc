// EXPECT: valid
// A texture_buffer passes to a helper by value like any texture; the helper reads, writes and sizes it.
// DISASM: OpTypeImage %float Buffer 2 0 0 1 Unknown
// DISASM: OpTypeImage %float Buffer 0 0 0 2 Unknown
#include <metal_stdlib>
using namespace metal;
float4 pick(texture_buffer<float, access::read> r, uint i) { return r.read(i % r.get_width()); }
void put(texture_buffer<float, access::read_write> w, float4 v, uint i) { w.write(v, i); }
kernel void k(texture_buffer<float, access::read> r [[texture(0)]], texture_buffer<float, access::read_write> w [[texture(1)]], uint t [[thread_position_in_grid]]) {
  put(w, pick(r, t), t);
}
