// EXPECT: error has the enum type Mode, and an integer does not convert to it
enum Mode { A, B };
struct S { Mode m; };
kernel void enum_struct_field_integer_rejected(device int *o [[buffer(0)]]) {
  S s;
  s.m = A;
  s.m = 1;
  o[0] = 1;
}
