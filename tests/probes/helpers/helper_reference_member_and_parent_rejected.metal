// EXPECT: error may overlap
// A member and its own struct overlap: a write through one is visible through the other in Apple, which a
// pair of copies cannot reproduce.
struct S { float a; float b; };
void f(thread float &m, thread S &s) { m = 7.0; s.b = 50.0; }
kernel void helper_reference_member_and_parent_rejected(device float *o [[buffer(0)]]) {
  S s; s.a = 1.0; s.b = 2.0;
  f(s.a, s);
  o[0] = s.b;
}
