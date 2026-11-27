// EXPECT: valid
// Increments, compound assignment and a plain expression inside a void cast all take effect.
kernel void void_cast_keeps_increment(device float *o [[buffer(0)]]) {
  int a = 1;
  (void)a++;
  (void)(++a);
  static_cast<void>(a += 100);
  (void)a;
  (void)(a + 1);
  (void)3;
  o[0] = float(a);
}
