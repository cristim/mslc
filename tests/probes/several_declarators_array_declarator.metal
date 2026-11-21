// EXPECT: valid
// RUN: grid=1 local=1
// "float a, b[4];" declares a scalar and an array; the bracket binds to b only.
kernel void several_declarators_array_declarator(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  float a = 1.0, b[4] = {1.0, 2.0, 3.0, 4.0}, c = 5.0;
  o[0] = a + b[i & 3] + c;
}
