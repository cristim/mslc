// EXPECT: valid
// A fetch-add whose value is not a literal. This is the probe that pins the
// operand order of OpAtomicIAdd, which is
//
//     Pointer, Memory (scope), Semantics (order), Value
//
// A literal 1 reads as a valid scope, so a module that puts the value second
// still validates when the value happens to be 1, which is how the wrong order
// survived the other probes. Any value that is not a small constant exposes it:
// with the value in the scope position, spirv-val rejects the module with
// "Scope ids must be OpConstant when Shader capability is present".
//
// DISASM: OpAtomicIAdd %uint
// DISASM-NOT: OpAtomicIAdd %int
//
// The operand order is pinned by this probe rather than by a needle, because
// the failure it guards is an ordering one and string(FIND) cannot compare
// positions. With the value in the scope position the module does not validate
// at all, which is the evidence: the wrong order fails here and passes in a
// probe whose value is the literal 1.
#include <metal_stdlib>
using namespace metal;
kernel void atomic_fetch_add_nonliteral_value(device atomic_uint *counter [[buffer(0)]],
                                             constant uint *addend [[buffer(1)]],
                                             device uint *out [[buffer(2)]],
                                             uint gid [[thread_position_in_grid]])
{
  // A value read from a buffer is never a constant the assembler can mistake
  // for a scope, which is what makes this the probe that pins the order.
  out[gid] = atomic_fetch_add_explicit(counter, addend[gid], memory_order_relaxed);
}