// EXPECT: valid
// RUN: grid=1 local=1
#include <metal_stdlib>
using namespace metal;
struct Box { component c; component a[2]; };
component pick(component x, thread component& r) { r = component::w; return x < r ? x : r; }
kernel void hit(device int* o [[buffer(0)]], uint u [[thread_position_in_grid]]) {
  component out = component::x; component res = pick(u == 0u ? component::y : component::z, out);
  Box b{ component::x, { component::y, component::z } };
  int sel = 0;
  switch (res) { case component::y: sel = 20; break; case component::z: sel = 30; break; default: sel = 40; break; }
  o[0] = sel + int(out == component::w) + int(b.a[0]) + int(b.c);
}
