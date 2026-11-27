// EXPECT: error argument 1 of the call to "mk" has the enum type Mode
enum Mode { A, B, C };
enum Other { P, Q };
void mk(Mode m) { }
kernel void enum_argument_other_enum_rejected(device int *o [[buffer(0)]]) {
  mk(P);
  o[0] = 1;
}
