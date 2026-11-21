// EXPECT: error redefinition of "i"
kernel void several_declarators_for_initialiser_redefinition_rejected(device float *o [[buffer(0)]]) {
  for (int i = 0, i = 1; i < 3; ++i) {
    o[0] = 1.0;
  }
}
