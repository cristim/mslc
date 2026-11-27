// EXPECT: valid
// DISASM-MATCH: OpSpecConstantFalse %bool
// DISASM-MATCH: OpSpecConstant %int 0
// DISASM-MATCH: OpSpecConstant %uint 0
// DISASM-MATCH: OpSpecConstant %float 0
// DISASM-MATCH: OpSpecConstant %half 0
// DISASM-MATCH: OpSpecConstant %short 0
// DISASM-MATCH: OpSpecConstant %ushort 0
// DISASM-MATCH: OpSpecConstant %char 0
// DISASM-MATCH: OpSpecConstant %uchar 0
// DISASM-MATCH: OpSpecConstant %long 0
// DISASM-MATCH: OpSpecConstant %ulong 0
// REFLECT: { "name": "cb", "type": "bool", "index": 0, "spec_id": 65536, "defined_spec_id": 131072 }
// REFLECT: { "name": "ci", "type": "int", "index": 1, "spec_id": 65537, "defined_spec_id": 131073 }
// REFLECT: { "name": "cu", "type": "uint", "index": 2,
// REFLECT: { "name": "cf", "type": "float", "index": 3,
// REFLECT: { "name": "ch", "type": "half", "index": 4,
// REFLECT: { "name": "cs", "type": "short", "index": 5,
// REFLECT: { "name": "cus", "type": "ushort", "index": 6,
// REFLECT: { "name": "cc", "type": "char", "index": 7,
// REFLECT: { "name": "cuc", "type": "uchar", "index": 8,
// REFLECT: { "name": "cl", "type": "long", "index": 9,
// REFLECT: { "name": "cul", "type": "ulong", "index": 10, "spec_id": 65546, "defined_spec_id": 131082 }
//
// Every scalar type a function constant can have, each a specialization constant of its own type.
#include <metal_stdlib>
using namespace metal;
constant bool cb [[function_constant(0)]];
constant int ci [[function_constant(1)]];
constant uint cu [[function_constant(2)]];
constant float cf [[function_constant(3)]];
constant half ch [[function_constant(4)]];
constant short cs [[function_constant(5)]];
constant ushort cus [[function_constant(6)]];
constant char cc [[function_constant(7)]];
constant uchar cuc [[function_constant(8)]];
constant long cl [[function_constant(9)]];
constant ulong cul [[function_constant(10)]];
kernel void k(device long* out [[buffer(0)]]) {
    out[0] = cb; out[1] = ci; out[2] = cu; out[3] = long(cf); out[4] = long(ch); out[5] = cs;
    out[6] = cus; out[7] = cc; out[8] = cuc; out[9] = cl; out[10] = long(cul);
}
