// EXPECT: error argument 1 of the call to "mk" has the enum type Mode
enum Mode { A, B, C };
void mk(Mode m) { }
kernel void enum_argument_integer_rejected(device int *o [[buffer(0)]]) {
  mk(1);
  o[0] = 1;
}
