// EXPECT: error excess elements in array initializer
constant float K[2] = {1.0, 2.0, 3.0};
kernel void file_scope_constant_array_excess_elements_rejected(device float *o [[buffer(0)]]) {
  o[0] = K[0];
}
