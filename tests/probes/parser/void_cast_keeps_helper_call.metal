// EXPECT: valid
// A helper called under a cast to void still runs, including a void helper.
float side(thread int &x) { x += 10; return 1.0; }
void nothing() {}
kernel void void_cast_keeps_helper_call(device float *o [[buffer(0)]]) {
  int a = 1;
  (void)side(a);
  (void)nothing();
  o[0] = float(a);
}
