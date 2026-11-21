// EXPECT: valid
// RUN: grid=1 local=1
// DISASM: OpConstant %ulong 48
// DISASM: OpConstant %ulong 16
// DISASM-MATCH: OpIAdd %ulong %ulong_16 %[0-9]+
// DISASM-MATCH: OpUConvert %uint %ulong_16[_0-9]*
// DISASM-MATCH: OpIMul %ulong %[0-9a-z_]+ %ulong_48
//
// sizeof(type) is a ulong constant in a function body: it multiplies at ulong width, and
// narrows to a uint where it initialises one (a constant that fits, so a brace accepts it).
// Apple's compiler reads 48 and 16 for these types.
#include <metal_stdlib>
using namespace metal;
struct Rec { float3 a; float3 b; float c; };
kernel void sizeof_value_in_function_body(device ulong *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[0] = ulong(i) * sizeof(Rec);
    uint narrow = sizeof(float4) + 4;
    uint braced{sizeof(float4)};
    out[1] = narrow + braced;
}
