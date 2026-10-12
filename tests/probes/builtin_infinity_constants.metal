// EXPECT: valid
// INFINITY, HUGE_VALF and HUGE_VALH expand to compiler builtins; each is the value its name says
// (Apple GPU and lavapipe agree: inf, -inf, inf, and min/max against +-INFINITY give the finite operand).
kernel void builtin_infinity_constants(device float *o [[buffer(0)]], constant float *a [[buffer(1)]]) {
  o[0] = INFINITY;
  o[1] = -INFINITY;
  o[2] = HUGE_VALF;
  o[3] = min(a[0], INFINITY);
  o[4] = max(a[0], -INFINITY);
  half h = HUGE_VALH;
  o[5] = float(h);
  float2 lo = float2(INFINITY);
  o[6] = lo.y;
}
