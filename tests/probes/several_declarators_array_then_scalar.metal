// EXPECT: valid
// RUN: grid=1 local=1
// "float b[2], c;" : c is a scalar, the array length is not inherited.
kernel void several_declarators_array_then_scalar(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  float b[2] = {1.0, 2.0}, c = 5.0;
  o[0] = b[i & 1] + c;
}
