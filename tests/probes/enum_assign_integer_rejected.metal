// EXPECT: error has the enum type Mode, and an integer does not convert to it
// Apple: "cannot initialize a variable of type Mode with an rvalue of type int".
enum Mode { A, B, C };
kernel void enum_assign_integer_rejected(device int *o [[buffer(0)]]) {
  Mode m = A;
  m = 2;
  o[0] = 1;
}
