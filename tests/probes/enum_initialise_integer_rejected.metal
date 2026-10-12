// EXPECT: error has the enum type Mode, and an integer does not convert to it
enum Mode { A, B, C };
kernel void enum_initialise_integer_rejected(device int *o [[buffer(0)]]) {
  Mode m = 1;
  o[0] = 1;
}
