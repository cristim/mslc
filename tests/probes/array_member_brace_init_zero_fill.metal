// EXPECT: valid
// DISASM-MATCH: OpCompositeConstruct %_arr_float_uint_3 %float_1 %float_0 %float_0_0
// DISASM-MATCH: OpCompositeConstruct %_struct_[0-9]+ %[0-9]+ %float_0_1
//
// A short list for the array member zero-fills the rest of the array, and a
// member left out of the list is zero.
struct Q { float a[3]; float s; };

kernel void array_member_brace_init_zero_fill(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  Q q{{1.0f}};
  o[i] = q.a[2] + q.s;
}
