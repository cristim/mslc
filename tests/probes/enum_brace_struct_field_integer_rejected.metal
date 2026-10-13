// EXPECT: error an element of the initializer list has the enum type Mode
enum Mode { A, B, C };
struct S { Mode m; int n; };
kernel void enum_brace_struct_field_integer_rejected(device int *o [[buffer(0)]]) {
  S s = { 1, 1 };
  o[0] = s.n;
}
