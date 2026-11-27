// EXPECT: valid
// REFLECT: { "name": "x", "type": "int", "index": 0, "spec_id": 65536, "defined_spec_id": 131072 }
//
// A derived constant may read is_function_constant_defined through a constant that names it. On an RTX 3060 the
// kernel writes 8 with x not given, 7 with x = 5 and 8 with x = 1.
#include <metal_stdlib>
using namespace metal;
constant int x [[function_constant(0)]];
constant float fx [[function_constant(1)]];
constant bool d = is_function_constant_defined(x);
constant bool y = d && x > 2;
kernel void k(device int* out [[buffer(0)]]) { out[0] = y ? 7 : 8; }
