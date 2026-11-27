// EXPECT: error has to name a local array
// Apple accepts a row of a 2-D array (w(b[1])); mslc names only a whole local array.
void w(float m[3]) { m[1] = 2.0; }
kernel void helper_array_parameter_row_argument_rejected(device float *o [[buffer(0)]]) {
  float b[3][3] = {};
  w(b[1]);
  o[0] = b[1][1];
}
