// EXPECT: error round takes float or half scalars or vectors
// Apple: "call to 'round' is ambiguous" for an int argument.
kernel void math_round_int_rejected(device float *o [[buffer(0)]], constant int *in [[buffer(1)]]) {
  o[0] = round(in[0]);
}
