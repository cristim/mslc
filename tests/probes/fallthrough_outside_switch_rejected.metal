// EXPECT: error fallthrough annotation is outside switch statement
// Apple: "fallthrough annotation is outside switch statement".
kernel void fallthrough_outside_switch_rejected(device int *o [[buffer(0)]]) {
  o[0] = 1;
  [[fallthrough]];
}
