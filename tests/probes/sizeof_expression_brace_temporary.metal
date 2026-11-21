// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 8[^0-9]
// DISASM-MATCH: OpConstant %ulong 48[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 4[^0-9]
//
// A braced temporary of a struct is that struct's size, 8 for { float a; int b; } and 48 for one
// with a float3x3; its elements are not evaluated. Apple: 8 and 48.
#include <metal_stdlib>
using namespace metal;
struct Pair { float a; int b; };
struct Basis { float3x3 m; };
kernel void sizeof_expression_brace_temporary(device ulong *out [[buffer(0)]])
{
    int i = 0;
    out[0] = sizeof(Pair{ 1.0f, i++ });
    out[1] = sizeof(Basis{ float3x3(1.0f) });
    out[2] = i;
}
