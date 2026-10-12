// EXPECT: valid
// A negative enumerator is built as a negation by the parser and is still its enum.
enum Mode { N1 = -1, Z = 0, P = 5 };
kernel void enum_negative_enumerator_accepted(device int *o [[buffer(0)]]) {
  Mode m = N1;
  m = P;
  Mode n = Z;
  m = N1;
  o[0] = int(m) + int(n);
}
