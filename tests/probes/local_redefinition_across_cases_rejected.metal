// EXPECT: error redefinition of "a" in the same scope
// The cases of a switch share one scope.
kernel void local_redefinition_across_cases_rejected(device float *o [[buffer(0)]], uint p [[thread_position_in_grid]]) {
  switch (p) {
    case 0: float a; break;
    case 1: float a; break;
  }
  o[0] = 1.0;
}
