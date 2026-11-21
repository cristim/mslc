#include <metal_stdlib>
using namespace metal;
// An unfixed enum with an enumerator past INT_MAX has unsigned underlying type, so
// comparing it is an unsigned comparison (#107).
enum Mode { kSmall = 1, kBig = 0x80000000 };
kernel void k(device const uint* in [[buffer(0)]], device uint* o [[buffer(1)]],
	uint i [[thread_position_in_grid]]) {
	Mode m = Mode(in[i] | 0x80000000u);
	o[i] = (m > kSmall) ? 1u : 0u;
}
