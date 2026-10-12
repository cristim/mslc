// EXPECT: valid
// A member read straight off a struct-valued call: the call is made and the field taken
// from the struct it returns (lavapipe readback: 7.5 for in[0] = 2.5, matching the host).
struct S { float x; S(float v) : x(v + 5.0f) {} };
kernel void helper_result_member_constructor(device float *out [[buffer(0)]], constant float *in [[buffer(1)]]) {
  out[0] = S(in[0]).x;
  out[1] = static_cast<S>(in[0]).x;
}
