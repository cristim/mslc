// EXPECT: valid
// DISASM-MATCH: OpLoopMerge
// Both names are scoped to the loop; the second sees the first.
kernel void several_declarators_for_initialiser(device float *o [[buffer(0)]]) {
  for (int i = 0, j = i + 10; i < 3; ++i) {
    o[i] = float(j);
  }
  for (int i = 5, j = 6; i < 7; ++i) {
    o[i] = float(j);
  }
}
