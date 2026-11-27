// EXPECT: valid
// REFLECT: { "name": "b", "type": "bool", "index": 2, "spec_id": 65538, "defined_spec_id": 131074 }
//
// On a file-scope constant [[function_constant]] takes the index as a constant integer expression, which may name
// another constant; it is not that constant's gate.
#include <metal_stdlib>
using namespace metal;
constant int kIdx = 2;
constant bool b [[function_constant(kIdx)]];
kernel void k(device int* out [[buffer(0)]]) { out[0] = b; }
