// EXPECT: error is made after the class template
// Refs #383: N::h(v) reads N; a using directive written in N below the template makes Apple keep N::h(float) (1.0)
// where mslc bound the later M::h(int) (7.0), for an instance made directly as well as a nested one.
#include <metal_stdlib>
using namespace metal;
namespace N { float h(float x) { return 1.0f; } }
namespace M { float h(int x) { return 7.0f; } }
template <typename T> struct In { T v; float g() const { return N::h(v); } };
namespace N { using namespace M; }
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) {
	In<int> a; a.v = s; o[0] = a.g();
}
