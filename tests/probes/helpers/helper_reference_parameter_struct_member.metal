// EXPECT: valid
// A member of a local struct is passed through a temporary that is copied back.
struct S { float a; float b; };
void setf(float &x) { x = 9.0; }
kernel void helper_reference_parameter_struct_member(device float *o [[buffer(0)]]) {
  S s;
  s.a = 1.0;
  s.b = 2.0;
  setf(s.b);
  o[0] = s.b;
}
