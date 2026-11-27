// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %[^ ]+ 1 Offset 4
// DISASM-MATCH: OpMemberDecorate %[^ ]+ 4 Offset 32
// Fields declared together take the member type each and lay out in order.
struct S { float x, y; float2 pad; float4 v, w; };
kernel void several_declarators_struct_fields(device S *o [[buffer(0)]]) {
  o[0].y = o[0].x;
  o[0].w = o[0].v;
}
