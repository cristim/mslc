// EXPECT: valid
// A kernel's workgroup size is OpExecutionModeId LocalSizeId, not OpExecutionMode,
// and a vertex entry in the same module has no execution modes at all.
// DISASM-ENTRY-MODES: k LocalSizeId % % %, SignedZeroInfNanPreserve 32
// DISASM-ENTRY-MODES: v SignedZeroInfNanPreserve 32
// DISASM-ENTRY-BUILTINS: k GlobalInvocationId
struct V { float4 p [[position]]; };
kernel void k(device uint* o [[buffer(0)]], uint i [[thread_position_in_grid]]) { o[i] = i; }
vertex V v() { V o; o.p = float4(0.0); return o; }
