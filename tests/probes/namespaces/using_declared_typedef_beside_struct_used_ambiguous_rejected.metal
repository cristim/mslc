// EXPECT: error reference to "T" is ambiguous: it names A::T and T
// Regression (cross-agent review of #264, round 2): a typedef does not hide a struct tag of
// the same name the way a function, variable or enumerator does. "struct T{}; using A::T;"
// (A::T a typedef) is accepted at declaration (the widened tag exemption), but using "T" as a
// type is still ambiguous between the own struct and the using-brought typedef, as Apple
// reports it: collectOwn's hasNonTag/hasHider check must not treat a typedef as a hider.
#include <metal_stdlib>
using namespace metal;
namespace A { typedef int T; }
struct T { int q; };
using A::T;
kernel void k(device int* out [[buffer(0)]]) {
	T t = 3;
	out[0] = t;
}
