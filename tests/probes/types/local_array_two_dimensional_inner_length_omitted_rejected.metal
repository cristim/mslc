// EXPECT: error only the outermost array length may be omitted
kernel void local_array_two_dimensional_inner_length_omitted_rejected(device float *o [[buffer(0)]]) {
  float a[2][];
  o[0] = 0.0;
}
