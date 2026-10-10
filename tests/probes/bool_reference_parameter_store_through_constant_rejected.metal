// EXPECT: error cannot store through
// A constant reference is read only, bool or not.
kernel void bool_reference_parameter_store_through_constant_rejected(device float *o [[buffer(0)]], constant bool &b [[buffer(1)]]) {
  b = true;
  o[0] = 1.0;
}
