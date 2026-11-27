// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 16
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 32
// DISASM-MATCH: OpDecorate %_runtimearr__struct_[0-9]+ ArrayStride 64
//
// A struct with array members copied between a device buffer, a constant
// reference and a local. The laid-out and the value forms of the struct differ
// by the stride on the array, so the copy goes element by element. float4 then
// float[4] (16 bytes) then float2x2[2] (16 bytes each, 32 in all): 64 bytes.
struct Cell { float4 head; float w[4]; float2x2 m[2]; };
kernel void array_member_struct_copy(device Cell *cells [[buffer(0)]], constant Cell &c [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  Cell l = cells[i];
  l.w[1] = 5.0f;
  Cell k = c;
  l.w[0] = l.m[1][1].x + k.w[2];
  cells[i] = l;
}
