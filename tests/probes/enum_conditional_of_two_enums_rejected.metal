// EXPECT: error has the enum type Mode, and an integer does not convert to it
enum Mode { A, B, C };
enum Other { P, Q };
kernel void enum_conditional_of_two_enums_rejected(device int *o [[buffer(0)]], uint u [[thread_position_in_grid]]) {
  bool g = u > 0;
  Other w = P;
  Mode m = g ? A : w;
  o[0] = 1;
}
