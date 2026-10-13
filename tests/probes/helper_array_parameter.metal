// EXPECT: valid
// An array parameter decays to a pointer to the caller's array: a write is visible to the caller, and
// the first length is not part of the type (float d[3] takes the four-element array). Lavapipe
// readback against Apple's GPU: big[2] is 3, big[3] is 0 (not reached), f[0] is 11, f[2] is 12.
void fill(float d[], float v) { for (int i = 0; i < 3; i++) d[i] = v + i; }
void forward(float d[3]) { fill(d, 10.0); d[0] += 1.0; }
kernel void helper_array_parameter(device float *o [[buffer(0)]]) {
  float big[4] = {};
  fill(big, 1.0);
  float f[3];
  forward(f);
  o[0] = big[2];
  o[1] = big[3];
  o[2] = f[0];
  o[3] = f[2];
}
