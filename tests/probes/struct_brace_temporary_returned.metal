// EXPECT: valid
// S{...} is a value of the struct: returned, passed, assigned and read (lavapipe readback of the
// kernel in the PR: 4.5 4 7.5 2.5 0 4.5 5; Apple accepts every form).
struct P { float a; float4 v; };
P mk(float x) { return P{x * 2.0, float4(x, x + 1.0, x + 2.0, x + 3.0)}; }
kernel void struct_brace_temporary_returned(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  o[0] = mk(in[0]).a;
  o[1] = mk(in[0]).v.z;
}
