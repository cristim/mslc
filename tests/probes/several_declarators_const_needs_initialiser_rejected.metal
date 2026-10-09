// EXPECT: error the const variable "b" needs an initialiser
kernel void several_declarators_const_needs_initialiser_rejected(device float *o [[buffer(0)]]) {
  const float a = 1.0, b;
  o[0] = a;
}
