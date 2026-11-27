// EXPECT: valid
// DISASM: OpAtomicExchange
#include <metal_stdlib>
kernel void k(device metal::atomic_int *a [[buffer(0)]],device int *o [[buffer(1)]]){o[0]=metal::atomic_exchange_explicit(a,o[1],metal::memory_order_relaxed);}
