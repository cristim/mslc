// EXPECT: error an array of pointers is not lowered yet
kernel void several_declarators_array_of_pointers_rejected(device float *o [[buffer(0)]]) {
  float a = 1.0, *c[2];
  o[0] = a;
}
