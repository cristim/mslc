// EXPECT: valid
// DISASM-MATCH: OpTypeArray %_arr_float_uint_3 %uint_2
// DISASM-MATCH: OpAccessChain %_ptr_Function_float %[0-9]+ %[0-9]+ %[0-9]+
// Nested braces, a dynamic row and column, and a store through the same chain. Lavapipe readback
// against Apple's GPU: a[1][2] is 106 after the store, a[0][1] is 2.
kernel void local_array_two_dimensional(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  float a[2][3] = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
  uint row = i + 1;
  uint column = i + 2;
  a[row][column] = a[row][column] + 100.0;
  o[0] = a[1][2];
  o[1] = a[0][1];
}
