// EXPECT: error "auto" is only lowered for a local variable
// Apple: auto not allowed in function prototype.
float twice(auto x) { return x * 2.0f; }
kernel void auto_helper_parameter_rejected(device float *o [[buffer(0)]]) {
  o[0] = twice(1.0f);
}
