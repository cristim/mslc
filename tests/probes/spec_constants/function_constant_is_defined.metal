// EXPECT: valid
// DISASM-CAPTURE: OpDecorate (%[0-9]+) SpecId 131072
// DISASM-MATCH-CAPTURED: OpBranchConditional $1 
// DISASM-CAPTURE: OpDecorate (%[0-9]+) SpecId 131073
// DISASM-MATCH-CAPTURED: OpSelect %int $2 
// REFLECT: { "name": "guarded", "type": "int", "index": 0, "spec_id": 65536, "defined_spec_id": 131072 }
// REFLECT: { "name": "flag", "type": "bool", "index": 1, "spec_id": 65537, "defined_spec_id": 131073 }
//
// is_function_constant_defined(c) is the bool at c's second SpecId, which the host sets when it gives c
// a value; "constant bool b = is_function_constant_defined(c);" names the same bool.
#include <metal_stdlib>
using namespace metal;
constant int guarded [[function_constant(0)]];
constant bool flag [[function_constant(1)]];
constant bool has_guarded = is_function_constant_defined(guarded);
kernel void k(device int* out [[buffer(0)]]) {
    if (has_guarded) {
        out[0] = guarded;
    }
    out[1] = is_function_constant_defined(flag) ? 1 : 0;
}
