// EXPECT: valid
// DISASM-MATCH: OpExtInst %float %[A-Za-z0-9_]+ FSign
kernel void math_sign_float(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  o[0] = sign(in[0]);
}
