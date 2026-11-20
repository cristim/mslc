// EXPECT: valid
// RUN: grid=1 local=1
// Refs #385: a member named A and a namespace alias A declared below the template: the template names no namespace, so the using directive in N above it is not a late one. Apple gives 1.0.
#include <metal_stdlib>
using namespace metal;
namespace N { float h(float x) { return 1.0f; } }
namespace M { float h(int x) { return 7.0f; } }
namespace N { using namespace M; }
template <typename T> struct In { T v; float A; float g() const { return A + 1.0f; } };
namespace A = N;
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) {
	In<int> a; a.v = s; a.A = 0.0f; o[0] = a.g();
}
