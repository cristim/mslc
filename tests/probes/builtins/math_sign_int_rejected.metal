// EXPECT: error sign takes float or half scalars or vectors
kernel void math_sign_int_rejected(device float *o [[buffer(0)]], constant int *in [[buffer(1)]]) {
  o[0] = sign(in[0]);
}
