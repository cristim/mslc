// EXPECT: error an element of the initializer list has the enum type Mode
enum Mode { A, B, C };
kernel void enum_brace_scalar_integer_rejected(device int *o [[buffer(0)]]) {
  Mode m = { 1 };
  o[0] = 1;
}
