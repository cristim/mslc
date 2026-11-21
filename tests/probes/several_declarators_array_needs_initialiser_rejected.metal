// EXPECT: error has no size and needs an initialiser list
kernel void several_declarators_array_needs_initialiser_rejected(device float *o [[buffer(0)]]) {
  float a = 1.0, c[];
  o[0] = a;
}
