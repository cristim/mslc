// EXPECT: error is an array, which is not a value
// Apple: "assigning to float from incompatible type float[2]". Without this check the module was invalid.
kernel void local_array_as_value_rejected(device float *o [[buffer(0)]]) {
  float a[2] = {1.0, 2.0};
  o[0] = a;
}
