// EXPECT: valid
// A bool and an int field of a returned struct.
struct Q { bool f; int n; };
Q mk(int a) { Q q; q.f = a > 1; q.n = a * 3; return q; }
kernel void helper_result_member_bool_and_int(device float *out [[buffer(0)]], constant int *in [[buffer(1)]]) {
  out[0] = mk(in[0]).f ? 1.0 : 0.0;
  out[1] = float(mk(in[0]).n);
}
