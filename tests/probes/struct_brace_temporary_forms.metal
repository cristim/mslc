// EXPECT: valid
struct S { float a; float b; };
float sum(S s) { return s.a + s.b; }
kernel void struct_brace_temporary_forms(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  S s = S{in[0], 2.0};
  o[0] = s.a + s.b;
  o[1] = S{3.0, 4.0}.b;
  o[2] = sum(S{in[0], 5.0});
  S t = S{.a = 1.0, .b = in[0]};
  o[3] = t.b;
  S z = S{};
  o[4] = z.a + z.b;
}
