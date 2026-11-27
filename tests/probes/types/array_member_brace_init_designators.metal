// EXPECT: valid
// DISASM-MATCH: OpCompositeConstruct %_arr_int_uint_2 %int_1 %int_2
// DISASM-MATCH: OpCompositeConstruct %_struct_[0-9]+ %[0-9]+ %int_3
//
// A designator may name the array member, in any order.
struct Q { int a[2]; int s; };

kernel void array_member_brace_init_designators(device int *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  Q q{.s = 3, .a = {1, 2}};
  o[i] = q.a[1] + q.s;
}
