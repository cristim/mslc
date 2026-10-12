// EXPECT: error ++ and -- do not apply to the enum Mode
enum Mode { A, B, C };
kernel void enum_increment_rejected(device int *o [[buffer(0)]]) {
  Mode m = A;
  m++;
  o[0] = 1;
}
