// EXPECT: valid
// RUN: grid=3 local=3
//
// A struct with an array member passed to a helper by value, after being copied
// out of a constant reference and edited.
struct P { float s; float a[3]; };
float f(P p, uint i) { return p.a[i] * p.s; }
kernel void array_member_by_value_helper(device float *o [[buffer(0)]], constant P &p [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  P q = p;
  q.a[1] = 10.0f;
  o[i] = f(q, i) + f(p, i);
}
