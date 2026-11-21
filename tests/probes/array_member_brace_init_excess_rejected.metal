// EXPECT: error excess elements in array initializer
//
// Apple rejects a list with more elements than the array member has.
struct Q { int a[2]; int s; };

kernel void array_member_brace_init_excess_rejected(device int *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  Q q{{1, 2, 3}, 4};
  o[i] = q.s;
}
