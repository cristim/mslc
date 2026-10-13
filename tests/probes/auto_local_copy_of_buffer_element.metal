// EXPECT: valid
// Changing the auto copy leaves the buffer element alone.
kernel void auto_local_copy_of_buffer_element(device float *o [[buffer(0)]], device float *in [[buffer(1)]]) {
  auto el = in[1];
  el += 1.0f;
  o[0] = in[1] + el * 0.0f;
}
