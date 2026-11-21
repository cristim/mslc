// EXPECT: valid
// An element or member passed by reference is a copy written back after the call. Another argument that
// is a call, a member call or a texture read cannot reach that storage unless it names it and takes it by
// reference or pointer, so none of these is refused (Apple: acc(s.a, rd(x)) = 17, acc(s.a, rd(s.b)) = 15,
// acc(a[j], rd(rd(x))) = 34, acc(s.a, tt.get()) = 5; values pinned by a read-back).
#include <metal_stdlib>
using namespace metal;
struct S { float a; float b; };
struct T { float v; float get() const { return v; } };
void acc(thread float &x, float v) { x += v; }
float rd(float v) { return v * 2.0f; }
float wr(thread float &y) { y = 3.0f; return 1.0f; }

kernel void k(device float *o [[buffer(0)]], texture2d<float> tx [[texture(0)]], sampler smp [[sampler(0)]]) {
  S s; s.a = 1.0f; s.b = 7.0f; float x = 8.0f; float a[3] = {1.0f, 2.0f, 3.0f}; int j = 1;
  T tt; tt.v = 4.0f; float2 uv = float2(0.5f);
  acc(s.a, rd(x));
  acc(s.b, rd(s.a));
  acc(a[j], rd(rd(x)));
  acc(s.a, tt.get());
  acc(s.a, tx.sample(smp, uv).x);
  o[0] = s.a + s.b + a[1];
}
