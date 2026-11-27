// EXPECT: error redefinition of "a" in the same scope
kernel void local_redefinition_in_one_declaration_rejected(device float *o [[buffer(0)]]) {
  float a, a;
  o[0] = 1.0;
}
