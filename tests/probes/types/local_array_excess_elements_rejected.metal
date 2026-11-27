// EXPECT: error excess elements in array initializer
// Apple: "excess elements in array initializer".
kernel void local_array_excess_elements_rejected(device float *o [[buffer(0)]]) {
  float a[2] = {1.0, 2.0, 3.0};
  o[0] = a[0];
}
