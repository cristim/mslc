// EXPECT: error excess elements in array initializer
// Apple: "excess elements in array initializer" for a fourth element of a row of three.
kernel void local_array_two_dimensional_excess_elements_rejected(device float *o [[buffer(0)]]) {
  float a[2][3] = {{1.0, 2.0, 3.0, 4.0}};
  o[0] = a[0][0];
}
