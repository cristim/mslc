// EXPECT: valid
// DISASM-MATCH: OpStore %[0-9]+ %float_1 Aligned 4
//
// "*p = v" stores to the first element, through the same address "p[0] = v" gets.
kernel void deref_writes_a_scalar(device float* out [[buffer(0)]])
{
    *out = 1.0;
}
