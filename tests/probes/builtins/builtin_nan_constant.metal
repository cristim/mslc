// EXPECT: valid
// NAN is not equal to itself; the comparison reads back 1 on Apple's GPU and on lavapipe.
kernel void builtin_nan_constant(device float *o [[buffer(0)]]) {
  float n = NAN;
  o[0] = (n != n) ? 1.0 : 0.0;
  o[1] = n;
}
