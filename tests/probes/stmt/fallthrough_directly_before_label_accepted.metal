// EXPECT: valid
// [[fallthrough]]; as the last statement before the next case or default, including an empty case,
// a nested one at the end of a case, and a nested switch.
kernel void fallthrough_directly_before_label_accepted(device int *o [[buffer(0)]], uint u [[thread_position_in_grid]]) {
  switch (u) {
    case 0: o[0] = 1; [[fallthrough]];
    case 1: [[fallthrough]];
    case 2: if (u) { [[fallthrough]]; }
    default: o[1] = 2;
  }
  switch (u) { case 0: switch (u) { case 1: [[fallthrough]]; case 2: break; } break; }
}
