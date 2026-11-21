// EXPECT: valid
// DISASM-MATCH: OpCompositeConstruct %_arr_int_uint_2 %int_2 %int_3
// DISASM-MATCH: OpCompositeConstruct %_struct_[0-9]+ %int_1 %[0-9]+
//
// An array member that follows a scalar member takes the second braces.
struct Q { int s; int a[2]; };

kernel void array_member_brace_init_after_scalar(device int *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  Q q{1, {2, 3}};
  o[i] = q.s + q.a[1];
}
