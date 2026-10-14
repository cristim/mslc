// EXPECT: valid
// DISASM: OpFunctionCall
// DISASM: OpIAdd
// DISASM-NOT: OpControlBarrier
#include <metal_stdlib>
namespace metal {int threadgroup_barrier(int v){return v+70;}}
kernel void k(device int *o [[buffer(0)]]){o[0]=metal::threadgroup_barrier(o[1]);}
