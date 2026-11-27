// EXPECT: valid
// DISASM: OpConstant %ulong 12
// DISASM-NOT: OpConstant %ulong 16
//
// An operator keeps a packed vector's type where the packed vector is the left operand or the
// only vector one, so sizeof is 12 although the value of one is a float3: packed + packed, packed
// + a float3 (the left operand decides), a packed vector beside a scalar of its element type
// either way round, a negation and a conditional with the packed vector first. Apple gives 12
// for every one; a 16 here is the unpacked value's size.
#include <metal_stdlib>
using namespace metal;
struct Rec { float3 a; packed_float3 b; };
kernel void sizeof_expression_packed_stays_packed(device ulong *out [[buffer(0)]],
    device Rec *recs [[buffer(1)]])
{
    float3 v = 0;
    bool yes = true;
    out[0] = sizeof(recs[0].b + recs[0].b);
    out[1] = sizeof(recs[0].b + v);
    out[2] = sizeof(recs[0].b * 2.0f);
    out[3] = sizeof(2.0f * recs[0].b);
    out[4] = sizeof(-recs[0].b);
    out[5] = sizeof(yes ? recs[0].b : v);
    out[6] = sizeof(recs[0].b - recs[0].a);
}
