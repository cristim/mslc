// EXPECT: valid
// Refs #341: a bare name, this-> and an explicit argument list inside a member template reach the object's members.
#include <metal_stdlib>
using namespace metal;
struct S {
	float a;
	float base() const { return a; }
	template <typename U> float f(U x) const { return base() + float(x); }
	template <typename U> float h(U x) const { return f(x) + this->f<float>(1.0f) + f<int>(2); }
	float viaMember() const { return f(2) + h(1.5f); }
};
float helper(S s, float y) { return s.f(y) + s.h(2) + s.viaMember(); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s{1.5f}; out[i] = helper(s, 1.0f); }
