// EXPECT: valid
kernel void auto_local_device_reference_to_buffer_element(device float *o [[buffer(0)]]) {
  device auto &r = o[1];
  r = 4.0f;
  device const auto &c = o[1];
  o[0] = c;
}
