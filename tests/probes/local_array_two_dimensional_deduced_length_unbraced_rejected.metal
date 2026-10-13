// EXPECT: error the first length of "a" is deduced from braced rows
// Apple counts the rows from a flat list; mslc refuses it rather than count them.
kernel void local_array_two_dimensional_deduced_length_unbraced_rejected(device float *o [[buffer(0)]]) {
  float a[][2] = {1.0, 2.0, 3.0};
  o[0] = a[1][0];
}
