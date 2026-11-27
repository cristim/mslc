// EXPECT: error the index 3 is outside the array "a" of 3
kernel void local_array_two_dimensional_constant_index_past_end_rejected(device float *o [[buffer(0)]]) {
  float a[2][3];
  o[0] = a[0][3];
}
