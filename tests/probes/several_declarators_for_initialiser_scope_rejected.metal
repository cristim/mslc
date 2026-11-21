// EXPECT: error "j" is not a parameter, local
kernel void several_declarators_for_initialiser_scope_rejected(device float *o [[buffer(0)]]) {
  for (int i = 0, j = 1; i < 3; ++i) {
    o[i] = float(j);
  }
  o[0] = float(j);
}
