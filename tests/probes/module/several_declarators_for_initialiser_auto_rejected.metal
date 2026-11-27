// EXPECT: error several declarators after auto are not lowered yet
kernel void several_declarators_for_initialiser_auto_rejected(device float *o [[buffer(0)]]) {
  for (auto i = 0, j = 1; i < 3; ++i) {
    o[i] = float(j);
  }
}
