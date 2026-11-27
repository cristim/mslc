// EXPECT: valid
// DISASM: OpFunctionCall
// DISASM: OpIAdd
// DISASM-NOT: OpAtomicStore
#include <metal_stdlib>
namespace metal {void atomic_store_explicit(thread int &v){v+=70;}}
kernel void k(device int *o [[buffer(0)]]){int v=o[1];metal::atomic_store_explicit(v);o[0]=v;}
