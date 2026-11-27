// EXPECT: valid
kernel void auto_local_thread_reference(device float *o [[buffer(0)]]) {
  float x = 1.0f;
  thread auto &r = x;
  r = 10.0f;
  o[0] = x;
}
