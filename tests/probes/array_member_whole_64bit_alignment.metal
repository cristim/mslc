// EXPECT: valid
// DISASM-MATCH: Aligned 8
//
// A whole array of structs holding a ulong, and a whole array of ulong, loaded from and stored to a buffer:
// the access is as aligned as the element (8). It was given 4, which spirv-val rejects for a 64-bit member.
#include <metal_stdlib>
struct B { ulong y; uint z; };
struct A { uint x; metal::array<B,2> bs; metal::array<ulong,2> u; };
kernel void array_member_whole_64bit_alignment(device ulong *out [[buffer(0)]], device A *a [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  metal::array<B,2> c = a[i].bs; a[i+1].bs = c; metal::array<ulong,2> d = a[i].u; a[i+1].u = d; out[i] = c[1].y + d[0]; }
