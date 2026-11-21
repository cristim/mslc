// EXPECT: valid
// A constant buffer's reference parameter, an element of a constant pointer, and a struct element of
// one all bind a constant reference parameter, including a struct (laid out as the buffer has it).
#include <metal_stdlib>
using namespace metal;
struct S { float a; float2 b; int i; };
float one(const constant float &c) { return c; }
float whole(const constant S &s) { return s.a + s.b.y + float(s.i); }
kernel void k(device float *o [[buffer(0)]], constant float &c [[buffer(1)]], constant float *p [[buffer(2)]],
	constant S *ss [[buffer(3)]], constant S &s [[buffer(4)]]) {
	o[0] = one(c) + one(p[1]) + one(s.a) + whole(ss[1]) + whole(s);
}
