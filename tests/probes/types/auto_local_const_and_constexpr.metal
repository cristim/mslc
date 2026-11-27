// EXPECT: valid
kernel void auto_local_const_and_constexpr(device float *o [[buffer(0)]]) {
  const auto kk = 3;
  constexpr auto cc = 4;
  auto z = kk * cc / 5;
  o[0] = z;
}
