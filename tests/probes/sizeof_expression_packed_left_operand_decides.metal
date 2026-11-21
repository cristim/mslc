// EXPECT: valid
// DISASM: OpConstant %ulong 16
// DISASM-NOT: OpConstant %ulong 12
//
// A float3 beside a packed_float3 is a float3, 16 bytes: the left operand's type decides, so
// float3 + packed and a conditional that has the float3 first are 16 (Apple: 16), where packed +
// float3 is 12 (sizeof_expression_packed_stays_packed).
#include <metal_stdlib>
using namespace metal;
struct Rec { float3 a; packed_float3 b; };
kernel void sizeof_expression_packed_left_operand_decides(device ulong *out [[buffer(0)]],
    device Rec *recs [[buffer(1)]])
{
    float3 v = 0;
    bool yes = true;
    out[0] = sizeof(v + recs[0].b);
    out[1] = sizeof(v / recs[0].b);
    out[2] = sizeof(yes ? v : recs[0].b);
    out[3] = sizeof(recs[0].a - recs[0].b);
}
