// EXPECT: valid
// DISASM-MATCH: Location 1
// Each field declarator in a list carries its own attribute list.
struct V { float4 pos [[position]]; float2 u [[user(locn0)]], v [[user(locn1)]]; };
vertex V several_declarators_struct_field_attributes(uint i [[vertex_id]]) {
  V o;
  o.pos = float4(0.0);
  o.u = float2(1.0);
  o.v = float2(2.0);
  return o;
}
