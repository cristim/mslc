// EXPECT: valid
// A local's own initializer resolves the local being declared, not an outer
// binding of the same name. `value` in the initializer names the int being
// declared here, which is uninitialized, so the value read is whatever the
// Function-storage slot held; Apple accepts this with only an uninitialized
// warning. The old behaviour resolved the file-scope float4 named `value` and
// reported "a scalar cannot be converted to a vector", rejecting a program
// Apple accepts.
//
// No numeric expectation is asserted on the value read: it is an uninitialized
// read, so asserting one would pin an undefined value rather than the lookup.
kernel void local_initializer_resolves_itself(device int *out [[buffer(0)]]) {
  int value = value;
  out[0] = value;
}