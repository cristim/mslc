// EXPECT: error has the enum type Mode, and an integer does not convert to it
enum Mode { A, B };
typedef Mode Alias;
kernel void enum_typedef_alias_integer_rejected(device int *o [[buffer(0)]]) {
  Alias m = 1;
  o[0] = 1;
}
