// EXPECT: error "K" is an array, which is not a value
constant float K[2] = {1.0, 2.0};
kernel void file_scope_constant_array_as_value_rejected(device float *o [[buffer(0)]]) {
  o[0] = K;
}
