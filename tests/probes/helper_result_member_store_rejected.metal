// EXPECT: error a member or an index has to start at a name
// Apple: "expression is not assignable": a returned struct is a temporary.
struct S { float x; };
S mk() { S s; s.x = 1.0; return s; }
kernel void helper_result_member_store_rejected(device float *out [[buffer(0)]]) {
  mk().x = 2.0;
  out[0] = 1.0;
}
