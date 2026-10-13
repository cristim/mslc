// EXPECT: error is not assignable
// Apple: "array type float[2] is not assignable".
kernel void local_array_assignment_rejected(device float *o [[buffer(0)]]) {
  float a[2] = {1.0, 2.0};
  float b[2];
  b = a;
  o[0] = b[1];
}
