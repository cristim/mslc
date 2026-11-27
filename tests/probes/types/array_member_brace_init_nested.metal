// EXPECT: valid
// DISASM-MATCH: OpCompositeConstruct %_arr_v2float_uint_2 %[0-9]+ %[0-9]+
// DISASM-MATCH: OpCompositeConstruct %_arr_int_uint_2_0 %int_5 %int_6
//
// An array member of vectors and a struct that holds a struct with an array
// member, each built from nested braces.
struct Q { int a[2]; };
struct R { float2 v[2]; Q q; int s; };

kernel void array_member_brace_init_nested(device int *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  R r{{float2(1.0f), float2(2.0f)}, {{5, 6}}, 7};
  o[i] = int(r.v[1].x) + r.q.a[1] + r.s;
}
