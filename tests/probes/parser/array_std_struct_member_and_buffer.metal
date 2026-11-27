// EXPECT: valid
// RUN: grid=32 local=8 cases=3 elems=2048
// A struct holding an array<float, 3> by value: a copy of the struct is independent, the whole
// member is read and assigned, and a struct in a buffer round-trips through the local layout.
#include <metal_stdlib>
using namespace metal;
struct W { array<float, 3> a; int n; };
W make(float x) { W w; w.a = {x, x + 1.0f, x + 2.0f}; w.n = 4; return w; }
kernel void array_std_struct_member_and_buffer(device W* ws [[buffer(0)]], device float* out [[buffer(1)]],
                                               device const float* in [[buffer(2)]],
                                               uint i [[thread_position_in_grid]])
{
    W w = make(in[i]);
    W v = w;
    v.a[0] = 50.0f;
    array<float, 3> x = w.a;
    x[1] = 60.0f;
    w.a[2] = 70.0f;
    ws[i] = v;
    ws[i + 32].a = x;
    ws[i + 32].n = w.n;
    array<float, 3> fromBuffer = ws[i].a;
    fromBuffer[0] += 1.0f;
    ws[i + 64].a = fromBuffer;
    ws[i + 64].a[1] = ws[i].a[1] + 0.5f;
    out[i * 6 + 0] = w.a[0];
    out[i * 6 + 1] = w.a[2];
    out[i * 6 + 2] = v.a[0] + v.a[2];
    out[i * 6 + 3] = x[1] + x[2];
    out[i * 6 + 4] = fromBuffer[0];
    out[i * 6 + 5] = float(w.a.size());
}
