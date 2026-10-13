// EXPECT: error the value "f" returns has the enum type Mode
enum Mode { A, B, C };
Mode f(int i) { return 1; }
kernel void enum_return_integer_rejected(device int *o [[buffer(0)]]) {
  Mode m = f(1);
  o[0] = (int)m;
}
