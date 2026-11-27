// EXPECT: valid
// DISASM-MATCH: OpExtInst %float %[0-9]+ Fma %
// DISASM-MATCH: OpExtInst %float %[0-9]+ FAbs %
// DISASM-MATCH: OpExtInst %float %[0-9]+ Sin %
// DISASM-MATCH: OpExtInst %float %[0-9]+ InverseSqrt %
// DISASM-MATCH: OpSelect %float
// DISASM-MATCH: OpConstant %ulong 4[^0-9]
#include <metal_stdlib>
using namespace metal;
namespace UserInt { using metal::precise::min; int min(int x, int y) { return x + y; } }
kernel void math_precise_and_fast_calls(
    device const float* in [[buffer(0)]], device float* out [[buffer(1)]], device ulong* sizes [[buffer(2)]])
{
    float a = precise::fma(in[0], in[1], in[2]);
    float b = metal::precise::fma(a, in[1], in[2]);
    float c = precise::abs(-2.5f + in[0]);
    float d = fast::sin(in[0]) + metal::fast::rsqrt(in[1]);
    float e = precise::select(a, b, a > b) + fast::select(c, d, false) + float(UserInt::min(1, 2));
    { using precise::fma; out[0] = fma(b, c, e); }
    sizes[0] = sizeof(precise::fma(1.0f, 2.0f, 3.0f)) + sizeof(precise::select(1.0f, 2.0f, true)) - 4ul;
}
