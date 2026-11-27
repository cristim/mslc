// EXPECT: valid
// A member read off a chain of member-function calls evaluates each receiver once: with
// s.x = 3 the three reads are 6, 12 and 24 (reviewed: an earlier version emitted the receiver
// chain twice per link and gave 48 for the second).
struct S { float x; S twice() const { S r; r.x = x * 2.0; return r; } };
kernel void helper_result_member_chained_receivers(device float *o [[buffer(0)]]) {
  S s;
  s.x = 3.0;
  o[0] = s.twice().x;
  o[1] = s.twice().twice().x;
  o[2] = s.twice().twice().twice().x;
}
