// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 4[^0-9]
// DISASM-MATCH: OpConstant %ulong 8[^0-9]
// DISASM-MATCH: OpConstant %ulong 12[^0-9]
// DISASM-MATCH: OpConstant %ulong 20[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 16[^0-9]
//
// The value of a comma is its right operand, and in an operand that is not evaluated the array
// stays an array: (0, f) is a float (4), (0, table) the whole float[5] (20), (0, grid[1]) a
// float[2] row (8) and (0, recs[0].b) a packed_float3 (12). Apple: 4, 20, 8 and 12.
#include <metal_stdlib>
using namespace metal;
struct Rec { float3 a; packed_float3 b; };
kernel void sizeof_expression_comma(device ulong *out [[buffer(0)]], device Rec *recs [[buffer(1)]])
{
    float f = 0;
    float table[5];
    float grid[3][2];
    out[0] = sizeof((0, f));
    out[1] = sizeof((0, table));
    out[2] = sizeof((0, grid[1]));
    out[3] = sizeof((0, recs[0].b));
}
