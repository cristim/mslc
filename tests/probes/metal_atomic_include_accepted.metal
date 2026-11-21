// EXPECT: valid
// DISASM: OpAtomicIAdd
// <metal_atomic> is a <metal_stdlib> sub-header: mslc already declares the atomic types,
// memory_order_relaxed and the atomic builtins, so the include reads as <metal_stdlib>.
#include <metal_atomic>
using namespace metal;
kernel void metal_atomic_include_accepted(device atomic_uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ atomic_fetch_add_explicit(out, i, memory_order_relaxed); }
