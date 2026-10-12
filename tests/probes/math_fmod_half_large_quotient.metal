// EXPECT: valid
// DISASM-MATCH: OpFConvert
// A half remainder is computed in float: a driver that forms the quotient in half answers
// infinity once it passes the half range (fmod(65504h, 6h) is 2 on Apple and was -inf on lavapipe).
kernel void math_fmod_half_large_quotient(device float *o [[buffer(0)]], constant float *a [[buffer(1)]], constant float *b [[buffer(2)]]) {
  o[0] = float(fmod(half(a[0]), half(b[0])));
}
