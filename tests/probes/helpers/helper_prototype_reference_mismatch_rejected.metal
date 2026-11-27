// EXPECT: error "f(thread float&)" is declared but never defined
// A reference parameter and a value parameter are different signatures (Apple: two
// functions). The call below the prototype names the reference one, which has no body.
void f(thread float &x);
kernel void helper_prototype_reference_mismatch_rejected(device float *o [[buffer(0)]]) {
  float a = 3.0;
  f(a);
  o[0] = a;
}
void f(float x) { x = 2.0; }
