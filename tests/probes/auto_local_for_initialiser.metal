// EXPECT: valid
kernel void auto_local_for_initialiser(device float *o [[buffer(0)]]) {
  auto acc = 0;
  for (auto j = 0; j < 4; j++) acc += j;
  o[0] = acc;
}
