// EXPECT: valid
kernel void local_array_half_bool_int(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  int idx = int(in[0]);
  half h[2] = {1.5h, 2.5h};
  bool f[2] = {true, false};
  int n[3] = {7, 8, 9};
  n[idx] = n[idx] * 2;
  o[0] = float(h[idx]) + (f[idx] ? 1.0 : 0.0) + float(n[1]);
}
