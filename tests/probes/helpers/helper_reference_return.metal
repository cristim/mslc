// EXPECT: valid
// RUN: grid=1 local=1
// A helper that returns a thread reference, read as a value: ANGLE's addAssignInt and subAssignInt return the
// parameter they assign, and the caller assigns the result back. Reading the result reads the referred object as
// the return leaves it. C++ gives out = 15 22 1 8 12, and so does an RTX 3060.
#include <metal_stdlib>
using namespace metal;
template <typename X, typename Y>
thread X& addAssign(thread X& x, Y y) { x = x + y; return x; }
thread int& bump(thread int& x) { x += 10; return x; }
struct S { int v; };
const thread S& pick(const thread S& a, const thread S& b) { return a.v > b.v ? a : b; }
kernel void helper_reference_return(device int* out [[buffer(0)]]) {
    int a = 5;
    a = bump(a);
    out[0] = a;
    int b = 1;
    const int r = bump(b);
    out[1] = r + b;
    uint c = 0xffffffffu;
    c = addAssign(c, 2u);
    out[2] = int(c);
    S s1{3}, s2{8};
    out[3] = pick(s1, s2).v;
    int d = 2;
    bump(d);
    out[4] = d;
}
