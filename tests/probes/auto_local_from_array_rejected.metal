// EXPECT: error is an array, which is not a value
// Apple deduces a pointer to the first element, which mslc does not lower as a local.
kernel void auto_local_from_array_rejected(device float *o [[buffer(0)]]) {
  float a[2] = {1.0, 2.0};
  auto b = a;
  o[0] = b[1];
}
