// EXPECT: valid
// The self-reference fix must not disturb an initializer that names an outer
// local, which is the case that was always correct. `outer` is in scope, and
// `inner` is declared from it, so out[0] is 7.
kernel void local_initializer_reads_outer_when_the_name_differs(device int *out [[buffer(0)]]) {
  int outer = 7;
  int inner = outer;
  out[0] = inner;
}