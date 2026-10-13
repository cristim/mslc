// EXPECT: valid
kernel void auto_local_parenthesised_and_braced_initialisers(device float *o [[buffer(0)]]) {
  auto p(5);
  auto q{7};
  o[0] = p / 2 + q / 2;
}
