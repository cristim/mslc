// EXPECT: error a local pointer declared with auto is not lowered yet
kernel void auto_local_pointer_rejected(device float *o [[buffer(0)]]) {
  device auto *p = o;
  p[1] = 2.0;
}
