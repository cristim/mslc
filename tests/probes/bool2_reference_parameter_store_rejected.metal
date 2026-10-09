// EXPECT: error is a bool reference parameter
kernel void bool2_reference_parameter_store_rejected(device bool2 &b [[buffer(0)]]) {
  b = bool2(true, false);
}
