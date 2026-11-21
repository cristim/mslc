// EXPECT: valid
// DISASM-MATCH: OpLoopMerge
// Both the initialiser and the increment of a for may be comma lists, with a
// cast to void in the increment, and the initialiser may declare several names.
kernel void for_comma_lists(device int *o [[buffer(0)]]) {
  int j = 0, n = 0;
  for (j = 0, n = 5; j < 3; (void)j++, n--) {
    o[j] = n;
  }
  for (int a = 0, b = 10; a < 2; a++, b--) {
    o[3 + a] = b;
  }
}
