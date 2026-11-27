// EXPECT: error is const, and parameter "m" is a pointer that can change it
// Apple: no matching function for call to 'w'.
void w(float m[3]) { m[1] = 2.0; }
kernel void helper_array_parameter_const_argument_rejected(device float *o [[buffer(0)]]) {
  const float b[3] = {1, 2, 3};
  w(b);
  o[0] = b[1];
}
