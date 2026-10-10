// EXPECT: valid
// A local's own initializer resolves the local being declared, not an outer
// binding of the same name. `value` in the initializer names the int being
// declared here, which is uninitialized. Apple accepts this with only an
// uninitialized warning.
//
// The old behaviour resolved the file-scope float4 named `value`, whose use made
// the initializer a scalar, and rejected the program with "a scalar cannot be
// converted to a vector".
//
// No numeric expectation is asserted on the value read: it is an uninitialized
// read, so asserting one would pin an undefined value rather than the lookup.
// What is pinned is that the lookup resolved the int being declared: both loads
// read one Function-storage int variable, and nothing loads the outer float4.
// DISASM: %38 = OpVariable %_ptr_Function_int Function
// DISASM: OpLoad %int %38
// DISASM-NOT: OpLoad %v4float
kernel void local_initializer_resolves_itself(device int *out [[buffer(0)]]) {
  int value = value;
  out[0] = value;
}