// EXPECT: error an array cannot be declared with auto
kernel void auto_local_array_declarator_rejected(device float *o [[buffer(0)]]) {
  auto a[2] = {1.0, 2.0};
  o[0] = a[1];
}
