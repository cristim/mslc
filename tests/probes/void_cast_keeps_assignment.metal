// EXPECT: valid
// The operand of a cast to void is evaluated and its value dropped: the stores happen
// (lavapipe readback: o = 3 6 6, matching Apple; dropping the operand would leave 0 0 1).
kernel void void_cast_keeps_assignment(device float *o [[buffer(0)]], device float *in [[buffer(1)]]) {
  static_cast<void>(o[0] = in[0]);
  (void)(o[1] = in[0] * 2.0);
  (void)(o[2] += 5.0);
}
