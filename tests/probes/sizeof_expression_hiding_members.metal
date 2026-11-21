// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 12[^0-9]
// DISASM-MATCH: OpConstant %ulong 16[^0-9]
// DISASM-MATCH: OpConstant %ulong 2[^0-9]
// DISASM-MATCH: OpConstant %ulong 1[^0-9]
//
// A struct's field hides a file-scope name of the same spelling inside its member functions (the
// packed_float3 a is 12 where the file-scope float4 a is 16), a local hides the field (a uchar
// a is 1), a namespace's own short a hides the file-scope one (2) and ::a steps over it to the
// file-scope float4 (16). Apple: 12, 1, 2, 16.
#include <metal_stdlib>
using namespace metal;
constant float4 a = float4(0.0f);
struct Holder {
    packed_float3 a;
    ulong field() const { return sizeof(a + a); }
    ulong local() const { uchar a = 0; return sizeof(a++); }
};
namespace n {
    constant short a = 0;
    ulong inner() { return sizeof(true ? a : a); }
    ulong outer() { return sizeof(::a + ::a); }
}
kernel void sizeof_expression_hiding_members(device ulong *out [[buffer(0)]])
{
    Holder h;
    out[0] = h.field();
    out[1] = h.local();
    out[2] = n::inner();
    out[3] = n::outer();
}
