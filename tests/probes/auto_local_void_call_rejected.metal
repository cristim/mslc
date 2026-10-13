// EXPECT: error returns void and has no value
// Apple: variable has incomplete type void.
void nothing() {}
kernel void auto_local_void_call_rejected(device float *o [[buffer(0)]]) {
  auto x = nothing();
  o[0] = 1.0;
}
