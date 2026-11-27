// EXPECT: valid
// Two members of one struct, two members of nested structs, two elements at different literal indices of a
// local array and the members of a reference parameter name different storage, so each is copied in and
// out on its own (Apple accepts all of them; these were refused as "the same variable"). The values are
// pinned by a produced-value read-back, with a negative control.
#include <metal_stdlib>
using namespace metal;
struct S { float a; float b; };
struct W { S s; S t; float arr[3]; };
void f(thread float &x, thread float &y) { x = x + 1.0f; y = y + 2.0f; }
void g(thread S &s) { f(s.a, s.b); }
kernel void helper_reference_disjoint_members_accepted(device float *o [[buffer(0)]]) {
  S s; s.a = 10.0f; s.b = 20.0f;
  f(s.a, s.b);
  W w; w.s = s; w.t = s; w.arr[0] = 1.0f; w.arr[1] = 2.0f; w.arr[2] = 3.0f;
  f(w.s.a, w.t.a);
  f(w.arr[0], w.arr[2]);
  float a[3] = {1.0f, 2.0f, 3.0f};
  f(a[0], a[2]);
  g(s);
  o[0] = s.a + s.b + w.s.a + w.t.a + w.arr[0] + w.arr[2] + a[0] + a[2];
}
