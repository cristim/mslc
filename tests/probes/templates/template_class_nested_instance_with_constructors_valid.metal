// EXPECT: valid
// RUN: grid=1 local=1
// DISASM-NOT: OpConstant %float 100
// Refs #383: the same with a user constructor on each level (Apple 1.0).
#include <metal_stdlib>
using namespace metal;
namespace N { float h(float x) { return 1.0f; } }
template <typename T> struct In { T v; In() : v(0) {} float g() const { return N::h(v); } };
namespace N { float h(int x) { return 100.0f; } }
template <typename T> struct Out { In<T> in; Out() {} float f() const { return in.g(); } };
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) {
	Out<int> a; a.in.v = s; o[0] = a.f();
}
