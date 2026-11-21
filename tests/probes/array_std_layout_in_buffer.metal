// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// array<T, N> members of a struct in a buffer lay out as T[N]: sizeof and every member offset match Apple's, through whole-member reads and writes.
#include <metal_stdlib>
using namespace metal;
struct W{array<float3,2> a; array<half,3> h; array<float2x2,2> m; char c;};
struct S{float x; int y;};
kernel void k(device const W* p [[buffer(0)]], device W* q [[buffer(1)]], device float* o [[buffer(2)]], uint i [[thread_position_in_grid]]) {
  array<float3,2> x = p[i].a; x[1].y = 3; q[i].a = x; array<half,3> h = p[i].h; h[2]=h[0]; q[i].h = h; array<float2x2,2> m = p[i].m; q[i].m = m; q[i].c = p[i].c;
  array<S,2> s = {{ {1.0f,2}, {3.0f,4} }}; s[1].x = 2; array<S,2> t = s; t[0].x=3; o[i]=sizeof(W)+sizeof(array<float3,2>)+sizeof(array<S,2>)+t[0].x+s[0].x;
}
