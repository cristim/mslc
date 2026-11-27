// EXPECT: error a compound assignment does not apply to the enum Mode
enum Mode { A, B, C };
kernel void enum_compound_assignment_rejected(device int *o [[buffer(0)]]) {
  Mode m = A;
  m += 1;
  o[0] = 1;
}
