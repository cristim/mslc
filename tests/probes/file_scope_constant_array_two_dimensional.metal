// EXPECT: valid
// DISASM-MATCH: OpTypeArray %_arr_float_uint_3 %uint_2
// Lavapipe readback against Apple's GPU: K[1][2] is 6, hk(1, 0) is 8, K[j - 1][j - 2] with j = 2 is 4.
constant float K[2][3] = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
float hk(int i, int j) { return K[i][j] * 2.0; }
kernel void file_scope_constant_array_two_dimensional(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  int j = int(i) + 2;
  o[0] = K[1][j];
  o[1] = hk(1, 0);
  o[2] = K[j - 1][j - 2];
}
