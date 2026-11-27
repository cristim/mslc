// EXPECT: valid
// DISASM-MATCH: OpTypeFunction %void %_ptr_Function__arr_float_uint_3 %_ptr_Function__arr_float_uint_3 %_ptr_Function__arr__arr_float_uint_3_uint_3
// float m[3][3] is a pointer to rows of three. Lavapipe readback against Apple's GPU: y is 7 16 25.
void mul(float dst[3], float vec[3], float mat[3][3]) {
  for (int m = 0; m < 3; m++)
    dst[m] = vec[0] * mat[m][0] + vec[1] * mat[m][1] + vec[2] * mat[m][2];
}
kernel void helper_array_parameter_two_dimensional(device float *o [[buffer(0)]]) {
  float M[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
  float x[3] = {1, 0, 2};
  float y[3];
  mul(y, x, M);
  o[0] = y[0];
  o[1] = y[1];
  o[2] = y[2];
}
