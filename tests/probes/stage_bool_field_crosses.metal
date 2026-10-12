// EXPECT: valid
// DISASM-MATCH: OpVariable %_ptr_Output_uint Output
// DISASM-MATCH: OpDecorate %[A-Za-z0-9_]+ Flat
// A bool crosses from the vertex to the fragment function as a flat unsigned integer that is 0 or 1
// (lavapipe draw: the fragment function sees true and false as written; Apple accepts it).
struct VOut { float4 position [[position]]; bool flag; bool2 pair; };
vertex VOut stage_bool_field_crosses_vs(uint vid [[vertex_id]]) { VOut o; o.position = float4(0.0, 0.0, 0.0, 1.0); o.flag = vid > 0; o.pair = bool2(true, vid > 0); return o; }
fragment float4 stage_bool_field_crosses_fs(VOut in [[stage_in]]) { return (in.flag && in.pair.x) ? float4(1.0) : float4(0.0); }
