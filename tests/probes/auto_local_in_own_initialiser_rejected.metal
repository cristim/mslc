// EXPECT: error cannot appear in its own initializer
// Apple: variable y declared with deduced type auto cannot appear in its own initializer.
kernel void auto_local_in_own_initialiser_rejected(device float *o [[buffer(0)]]) {
  auto y = y;
  o[0] = 1.0;
}
