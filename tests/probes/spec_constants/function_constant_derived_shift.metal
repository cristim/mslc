// EXPECT: valid
// DISASM-MATCH: OpSpecConstantOp %int ShiftRightArithmetic
// DISASM-NOT: ShiftRightLogical
//
// A shift has the type of its promoted left operand alone, not the usual arithmetic conversions of the two:
// x >> 1u is an int shifted arithmetically, and (x >> 1u) < 0 compares two ints. On an RTX 3060 with x = -8 the
// kernel writes -4 and 1.
constant int x [[function_constant(0)]];
constant int y = x >> 1u;
constant bool negative = (x >> 1u) < 0;
kernel void k(device int* out [[buffer(0)]]) { out[0] = y; out[1] = negative; }
