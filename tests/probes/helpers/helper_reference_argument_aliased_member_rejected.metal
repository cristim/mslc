// EXPECT: error passes the same variable through two reference parameters
// s.b is passed through a temporary that is copied back, so the write made through
// the whole struct would be lost; refused instead of giving the wrong value.
struct S { float a; float b; };
void f(thread float &m, thread S &s) { m = 7.0; s.b = 50.0; }
kernel void helper_reference_argument_aliased_member_rejected(device float *o [[buffer(0)]]) {
  S s;
  s.a = 1.0;
  s.b = 2.0;
  f(s.b, s);
  o[0] = s.b;
}
