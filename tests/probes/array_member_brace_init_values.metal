// EXPECT: valid
// DISASM-MATCH: OpCompositeConstruct %_arr_float_uint_3 %float_1 %float_2 %float_3
// DISASM-MATCH: OpCompositeConstruct %_struct_[0-9]+ %[0-9]+ %float_4
//
// A struct with an array member takes the array in its own braces, as
// "P q = {1.0f, {1.0f, 2.0f, 3.0f}}" does in Apple's compiler.
struct Q { float a[3]; float s; };

kernel void array_member_brace_init_values(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  Q q = {{1.0f, 2.0f, 3.0f}, 4.0f};
  Q r{{5.0f, 6.0f, 7.0f}, 8.0f};
  o[i] = q.a[1] + q.s + r.a[2];
}
