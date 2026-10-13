// EXPECT: valid
// #288 (cross-agent review of #264): a helper parameter named like a struct tag, with no
// namespace or using declaration anywhere, is just the parameter inside the helper's body.
#include <metal_stdlib>
using namespace metal;
struct S { int y; };
int h(int S) { S = S + 1; return S; }
kernel void k(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	out[0] = h(int(i));
}
