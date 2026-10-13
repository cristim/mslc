// EXPECT: valid
// DISASM-MATCH: OpVariable %_ptr_Private__arr_int_uint_4 Private
// DISASM-NOT: OpVariable %_ptr_Function__arr_int_uint_4
// A constant array is one Private variable with a constant initialiser, read in place with a
// dynamic index by the kernel and by a helper. Lavapipe readback against Apple's GPU: P[2 + 1]
// is 40 and hp(2) is 30.
constant int P[4] = {10, 20, 30, 40};
float hp(int j) { return float(P[j]); }
kernel void file_scope_constant_array(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  o[0] = float(P[i + 3]);
  o[1] = hp(2);
}
