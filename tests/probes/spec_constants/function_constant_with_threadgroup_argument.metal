// EXPECT: valid
// DISASM: SpecId 3
// DISASM: SpecId 65536
// DISASM: SpecId 131072
// REFLECT: "kind": "ThreadgroupMemory", "metal_index": 0, "spec_id": 3,
// REFLECT: { "name": "count", "type": "uint", "index": 0, "spec_id": 65536, "defined_spec_id": 131072 }
//
// A function constant at index 0 and a threadgroup argument take SpecIds of their own: the length of
// a threadgroup argument counts from 3 up, function constants start at 65536.
#include <metal_stdlib>
using namespace metal;
constant uint count [[function_constant(0)]];
kernel void k(device float* out [[buffer(0)]], threadgroup float* scratch [[threadgroup(0)]],
              uint i [[thread_position_in_threadgroup]]) {
    scratch[i] = float(count);
    threadgroup_barrier(mem_flags::mem_threadgroup);
    out[i] = scratch[0];
}
