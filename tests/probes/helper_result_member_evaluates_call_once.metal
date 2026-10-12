// EXPECT: valid
// DISASM-MATCH: OpFunctionCall
struct S { float x; };
S mk(thread float &c) { c = c + 1.0; S s; s.x = c * 2.0; return s; }
kernel void helper_result_member_evaluates_call_once(device float *o [[buffer(0)]]) {
  float c = 0.0;
  o[0] = mk(c).x;
  o[1] = c;
}
