// EXPECT: error round takes 1 argument
kernel void math_round_arity_rejected(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  o[0] = round(in[0], in[1]);
}
