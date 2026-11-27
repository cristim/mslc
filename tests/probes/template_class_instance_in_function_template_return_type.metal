// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpTypeStruct %float %float
// DISASM: OpTypeStruct %int %int
// A return type Out<T> on a function template's parameter: deduction reads the parameters only, and each
// instance the calls choose returns its own Out<float> or Out<int>.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Out { T color; T twice; };
template <typename T> static Out<T> make(T x) { Out<T> o; o.color = x; o.twice = x + x; return o; }
template <typename T> Out<T> wrap(T x) { return make(x + T(1)); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	Out<float> a = make(float(i));
	Out<int> b = make(int(i));
	Out<float> c = wrap(float(i));
	out[i] = a.twice + float(b.twice) * 10.0f + c.color * 100.0f;
}
