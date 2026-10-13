// EXPECT: error indexed this far is still an array, which is not a value
// Apple rejects "o[0] = a[1];" too (assigning float from float[3]).
kernel void local_array_two_dimensional_row_as_value_rejected(device float *o [[buffer(0)]]) {
  float a[2][3];
  o[0] = a[1];
}
