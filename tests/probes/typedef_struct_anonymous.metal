// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 16
// DISASM: ArrayStride 32
//
// typedef struct { ... } Name declares a struct called Name, with the layout a struct of that name has.
#include <metal_stdlib>
using namespace metal;
typedef struct
{
    float2 position;
    float4 color;
} Vertex;
kernel void typedef_struct_anonymous(device Vertex* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i].position = float2(1.0, 2.0); out[i].color = float4(3.0); }
