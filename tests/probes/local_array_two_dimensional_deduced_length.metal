// EXPECT: valid
// The first length is taken from the braced rows. Apple gives a[2][1] as 6.
kernel void local_array_two_dimensional_deduced_length(device float *o [[buffer(0)]]) {
  float a[][2] = {{1.0, 2.0}, {3.0, 4.0}, {5.0, 6.0}};
  o[0] = a[2][1];
}
