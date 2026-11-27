// EXPECT: valid
// DISASM: OpFunctionCall
// DISASM: OpIAdd
// DISASM-NOT: OpControlBarrier
#include <metal_stdlib>
namespace metal {void threadgroup_barrier(thread int &v){v+=70;}}
kernel void k(device int *o [[buffer(0)]]){int v=o[1];metal::threadgroup_barrier(v);o[0]=v;}
