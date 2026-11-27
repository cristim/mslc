// EXPECT: error is an array of float[4], and parameter "m" points at float[3]
// Apple: no matching function for call to 'w'; only the first length decays.
float w(float m[2][3]) { return m[1][2]; }
kernel void helper_array_parameter_inner_length_mismatch_rejected(device float *o [[buffer(0)]]) {
  float b[3][4] = {};
  o[0] = w(b);
}
