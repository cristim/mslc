// EXPECT: error argument 1 of the call to "S::S" has the enum type Mode
enum Mode { A, B, C };
struct S { Mode m; S(Mode v) : m(v) {} };
float read(S s) { return 0.0; }
kernel void enum_constructor_argument_integer_rejected(device float *o [[buffer(0)]]) {
  o[0] = read(S(1));
}
