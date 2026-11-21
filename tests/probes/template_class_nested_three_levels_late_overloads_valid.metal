// EXPECT: valid
// RUN: grid=1 local=1
// DISASM-NOT: OpConstant %float 1000
// Refs #383: three levels, an overload between each: every level binds what was declared above its own template (Apple 1001.1); the outermost order gave 1100.1.
#include <metal_stdlib>
using namespace metal;
namespace N { float h(float x) { return 1.0f; } }
template <typename T> struct In { T v; float g() const { return N::h(v); } };
namespace N { float h(int x) { return 100.0f; } }
template <typename T> struct Mid { In<T> in; float m() const { return in.g() + 10.0f * N::h(in.v); } };
namespace N { float h(uint x) { return 1000.0f; } }
template <typename T> struct Out { Mid<T> mid; float f() const { return mid.m() + 0.001f * N::h(mid.in.v); } };
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) {
	Out<int> a; a.mid.in.v = s; o[0] = a.f();
}
