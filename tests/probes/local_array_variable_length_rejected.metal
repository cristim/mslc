// EXPECT: error an array length needs a constant integer
// Apple: "array size is not a constant expression".
kernel void local_array_variable_length_rejected(device float *o [[buffer(0)]]) {
  int n = 3;
  float a[n];
  o[0] = 1.0;
}
