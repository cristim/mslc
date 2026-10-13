// EXPECT: error is an array of pointers, references, atomics or resources
float w(thread float *p[2]) { return 0.0; }
kernel void helper_array_of_pointers_parameter_rejected(device float *o [[buffer(0)]]) {
  o[0] = 1.0;
}
