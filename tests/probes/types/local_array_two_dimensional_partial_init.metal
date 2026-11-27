// EXPECT: valid
// A short row and a short list zero-fill the rest. Lavapipe readback against Apple's GPU:
// b[0][1] is 0, b[1][1] is 9, b[1][2] is 0, c[1][0] is 4, c[1][1] is 0.
kernel void local_array_two_dimensional_partial_init(device float *o [[buffer(0)]]) {
  float b[2][3] = {{7.0}, {8.0, 9.0}};
  float c[2][3] = {1.0, 2.0, 3.0, 4.0};
  o[0] = b[0][1];
  o[1] = b[1][1];
  o[2] = b[1][2];
  o[3] = c[1][0];
  o[4] = c[1][1];
}
