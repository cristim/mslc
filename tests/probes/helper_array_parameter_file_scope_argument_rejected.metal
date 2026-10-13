// EXPECT: error has to name a local array
// Apple: no matching function for call to 'w' (a constant array does not convert to a thread pointer).
void w(float m[3]) { m[1] = 2.0; }
constant float K[3] = {1, 2, 3};
kernel void helper_array_parameter_file_scope_argument_rejected(device float *o [[buffer(0)]]) {
  w(K);
  o[0] = K[1];
}
