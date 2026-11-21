// EXPECT: valid
// DISASM: %float_1_5
// DISASM-NOT: %float_7_5
// + - ~ on a uchar give an int, however the value is used afterwards (an auto local, a
// conditional in a qualified call): every call is f(int) (Apple), never f(uchar).
#include <metal_stdlib>
using namespace metal;
float f(int x) { return 1.5f; }
float f(uchar x) { return 7.5f; }
namespace N { float f(int x) { return 1.5f; } float f(uchar x) { return 7.5f; } }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	uchar a = 1;
	auto t = -a;
	out[0] = f(t) + f(+a) + f(~a) + N::f(i ? -a : -a) + N::f(i ? ~a : ~a);
}
