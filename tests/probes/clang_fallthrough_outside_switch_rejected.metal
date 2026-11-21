// EXPECT: error fallthrough annotation is outside switch statement
kernel void k(device int *o [[buffer(0)]]) {
  [[clang::fallthrough]];
  o[0] = 1;
}
