// EXPECT: error struct "S" has no member "nosuch"
struct S { float x; };
S mk() { S s; s.x = 1.0; return s; }
kernel void helper_result_member_unknown_rejected(device float *out [[buffer(0)]]) {
  out[0] = mk().nosuch;
}
