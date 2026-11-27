// EXPECT: error cannot store through "K"
// Apple: cannot assign to variable 'K' with const-qualified type.
constant float K[3] = {1.0, 2.0, 3.0};
kernel void file_scope_constant_array_store_rejected(device float *o [[buffer(0)]]) {
  K[0] = 4.0;
  o[0] = K[0];
}
