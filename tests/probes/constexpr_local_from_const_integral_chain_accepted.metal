// EXPECT: valid
// A const integral local with a constant initialiser stands in a constant expression
// in C++, also when the initialiser is more than a literal.
constant int K = 4;
kernel void constexpr_local_from_const_integral_chain_accepted(device float *out [[buffer(0)]]) {
  const int n = K * 2;
  constexpr int a = n;
  const int p = 3;
  const int q = p + 1;
  constexpr int b = q;
  const int r = (int)2.5f;
  constexpr int c = r;
  const bool flag = K > 2;
  constexpr int d = flag ? 1 : 2;
  out[0] = float(a + b + c + d);
}
