// EXPECT: error has the enum type Mode, and an integer does not convert to it
enum Mode { A, B, C };
kernel void enum_conditional_with_integer_arm_rejected(device int *o [[buffer(0)]], uint u [[thread_position_in_grid]]) {
  bool g = u > 0;
  Mode m = g ? A : 1;
  o[0] = 1;
}
