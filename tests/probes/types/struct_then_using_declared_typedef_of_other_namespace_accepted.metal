// EXPECT: valid
// Regression (cross-agent review of #264): a tag beside any other kind, not only a function
// or variable, is exempt when the using declaration comes after the name is already declared
// (Apple accepts; master did too). The reverse order stays rejected (see
// using_declared_typedef_then_own_struct_rejected and using_declared_struct_then_own_typedef_rejected).
#include <metal_stdlib>
using namespace metal;
namespace A { typedef int T; }
struct T { int q; };
using A::T;
kernel void k(device int* out [[buffer(0)]]) {
	out[0] = 1;
}
