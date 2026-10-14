// EXPECT: valid
// DISASM: OpAtomicStore
// DISASM: OpAtomicLoad
#include <metal_stdlib>
kernel void k(device metal::atomic_int *a [[buffer(0)]],device int *o [[buffer(1)]]){metal::atomic_store_explicit(a,o[1],metal::memory_order_relaxed);o[0]=metal::atomic_load_explicit(a,metal::memory_order_relaxed);}
