// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// <metal_pack> is read: mslc declares the 4x8 and 2x16 pack and unpack functions it holds.
#include <metal_pack>
kernel void metal_pack_include_accepted(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = metal::pack_float_to_unorm4x8(metal::unpack_unorm4x8_to_float(i)); }
