// EXPECT: valid
// RUN: grid=1 local=1
// Refs #383: the middle template's member sees N::h(float) only, the outer's sees h(int) too (Apple 106.0, mslc gave 205.0).
#include <metal_stdlib>
using namespace metal;
namespace N { float h(float x) { return 1.0f; } }
template <typename T> struct In { T v; float g() const { return 5.0f; } };
template <typename T> struct Mid { In<T> in; float m() const { return in.g() + N::h(in.v); } };
namespace N { float h(int x) { return 100.0f; } }
template <typename T> struct Out { Mid<T> mid; float f() const { return mid.m() + N::h(mid.in.v); } };
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) {
	Out<int> a; a.mid.in.v = s; o[0] = a.f();
}
