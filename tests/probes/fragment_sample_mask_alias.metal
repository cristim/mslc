// EXPECT: valid
// DISASM: BuiltIn SampleMask
typedef uint Mask;
struct O { Mask m [[sample_mask]]; };
fragment O f() { O o; o.m = 10u; return o; }
