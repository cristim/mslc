// EXPECT: valid
// DISASM-MATCH: OpSpecConstantOp %int IMul
// REFLECT: { "name": "x", "type": "int", "index": 0, "spec_id": 65536, "defined_spec_id": 131072 }
//
// A constant computed from a function constant is a specialization constant operation over it, which the driver
// works out from the value it is given; the kernel that reads y lists x among its function constants.
constant int x [[function_constant(0)]];
constant int y = x * 2;
kernel void k(device int* out [[buffer(0)]]) { out[0] = y; }
