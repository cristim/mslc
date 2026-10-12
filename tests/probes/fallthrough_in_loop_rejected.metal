// EXPECT: error fallthrough annotation is outside switch statement
kernel void fallthrough_in_loop_rejected(device int *o [[buffer(0)]]) {
  for (int i = 0; i < 2; i++) { [[fallthrough]]; }
  o[0] = 1;
}
