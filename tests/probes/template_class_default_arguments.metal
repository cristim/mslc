// EXPECT: valid
// DISASM-MATCH: OpFunction %float None %[_0-9a-zA-Z]+.*OpFunction %float None %[_0-9a-zA-Z]+
// DISASM-NO-MATCH: OpFunction %float None %[_0-9a-zA-Z]+.*OpFunction %float None %[_0-9a-zA-Z]+.*OpFunction %float None %[_0-9a-zA-Z]+
// Refs #341: the defaults fill the missing arguments, and P<> is P<float,2>.
#include <metal_stdlib>
using namespace metal;
template <typename T = float, int N = 2> struct P { T v[N]; float n() const { return float(N); } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	P<> a;
	P<int> b;
	P<float, 2> c;
	out[i] = a.n() + b.n() + c.n();
}
