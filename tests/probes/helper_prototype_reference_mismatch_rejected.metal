// EXPECT: error overloading
// A reference parameter and a value parameter are different signatures.
void f(thread float &x);
kernel void helper_prototype_reference_mismatch_rejected(device float *o [[buffer(0)]]) {
  float a = 3.0;
  f(a);
  o[0] = a;
}
void f(float x) { x = 2.0; }
