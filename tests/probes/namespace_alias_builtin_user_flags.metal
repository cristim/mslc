// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_7[_0-9]* Aligned
#include <metal_stdlib>
namespace N { namespace metal { enum mem_flags { mem_threadgroup = 7 }; } }
namespace M = N::metal;
kernel void k(device int* out [[buffer(0)]]) {
    out[0] = int(M::mem_flags::mem_threadgroup);
}
