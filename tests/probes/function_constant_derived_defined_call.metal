// EXPECT: valid
// REFLECT: { "name": "x", "type": "int", "index": 0, "spec_id": 65536, "defined_spec_id": 131072 }
//
// is_function_constant_defined may be an operand of a derived constant's initialiser. On an RTX 3060 the kernel
// writes 0 with x not given, 1 with x = 5 and 0 with x = 1.
constant int x [[function_constant(0)]];
constant bool y = is_function_constant_defined(x) && x > 2;
kernel void k(device int* out [[buffer(0)]]) { out[0] = y; }
