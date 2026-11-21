// EXPECT: error to close a member initialiser
// Apple rejects "a(x, y)" for a scalar member: a comma in a member initialiser
// separates arguments, it is not the comma operator.
struct S {
  float a;
  S(float x, float y) : a(x, y) {}
};
kernel void comma_member_initialiser_rejected(device float *o [[buffer(0)]]) {
  S s(1.0f, 2.0f);
  o[0] = s.a;
}
