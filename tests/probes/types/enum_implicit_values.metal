// EXPECT: valid
// REFLECT: "metal_index": 0, "descriptor": { "set": 0, "binding": 0 }, "member": 0
// REFLECT: "metal_index": 1, "descriptor": { "set": 0, "binding": 0 }, "member": 1
// REFLECT: "metal_index": 6, "descriptor": { "set": 0, "binding": 0 }, "member": 2
// DISASM: OpConstant %int 5
// DISASM: OpConstant %int 6
//
// Enumerators count up from 0 and from the last explicit value, and are the buffer index.
#include <metal_stdlib>
using namespace metal;
enum Slots { First, Second, Jump = 5, AfterJump };
kernel void enum_implicit_values(device uint* a [[buffer(First)]], device uint* b [[buffer(Second)]], device uint* c [[buffer(AfterJump)]], uint i [[thread_position_in_grid]])
{ a[i] = Jump; b[i] = AfterJump; c[i] = First + Second; }
