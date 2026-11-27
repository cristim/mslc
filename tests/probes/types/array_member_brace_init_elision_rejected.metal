// EXPECT: error brace elision into the array member a of Q
//
// Apple accepts Q q{1, 2, 3}; mslc asks for the braces around the array.
struct Q { int a[2]; int s; };

kernel void array_member_brace_init_elision_rejected(device int *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  Q q{1, 2, 3};
  o[i] = q.s;
}
