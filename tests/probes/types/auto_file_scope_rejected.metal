// EXPECT: error "auto" is only lowered for a local variable
// Apple accepts this; mslc lowers auto for locals only.
constant auto K = 1;
kernel void auto_file_scope_rejected(device float *o [[buffer(0)]]) {
  o[0] = K;
}
