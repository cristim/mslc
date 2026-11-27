// EXPECT: error has the enum type Mode, and an integer does not convert to it
enum Mode { A, B, C };
kernel void enum_initialise_unsigned_rejected(device int *o [[buffer(0)]], uint u [[thread_position_in_grid]]) {
  Mode m = u;
  o[0] = 1;
}
