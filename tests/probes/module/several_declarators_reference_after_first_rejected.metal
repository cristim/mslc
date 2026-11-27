// EXPECT: error a reference after the first declarator is not lowered yet
kernel void several_declarators_reference_after_first_rejected(device float *o [[buffer(0)]]) {
  float a = 1.0, &r = a;
  o[0] = r;
}
