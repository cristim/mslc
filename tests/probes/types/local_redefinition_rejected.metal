// EXPECT: error redefinition of "a" in the same scope
// Apple: "redefinition of 'a'".
kernel void local_redefinition_rejected(device float *o [[buffer(0)]]) {
  float a = 1.0;
  float a = 2.0;
  o[0] = a;
}
