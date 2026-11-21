// EXPECT: valid
// DISASM-MATCH: OpReturn
// DISASM-NOT: OpReturnValue
// DISASM-MATCH: OpStore %[0-9]+ %int_6[^_0-9]
// DISASM-MATCH: OpStore %[0-9]+ %int_7 Aligned
// "return (void)e;" in a void function evaluates e and returns nothing, in a
// helper and in a kernel alike.
void helper(thread int &n) {
  return (void)(n = 6);
}
kernel void return_void_cast_in_void(device int *o [[buffer(0)]]) {
  int n = 0;
  helper(n);
  o[0] = n;
  return (void)(o[1] = 7);
}
