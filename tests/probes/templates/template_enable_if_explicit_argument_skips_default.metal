// EXPECT: valid
// RUN: grid=1 local=1
// DISASM-MATCH: OpFunctionCall %int
// Refs #341: a template argument the call names is not substituted from its default: f<int, int>
// gives E itself, so the enable_if that would drop f<int> is never evaluated and the template is
// called (Apple does the same).
#include <metal_stdlib>
using namespace metal;
template <typename T, typename E = typename enable_if<is_floating_point<T>::value>::type>
T plusOne(T x) { return x + T(1); }
kernel void k(device int* out [[buffer(0)]])
{
	out[0] = plusOne<int, int>(3);
}
