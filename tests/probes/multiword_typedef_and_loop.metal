// EXPECT: valid
// DISASM: OpTypeInt 16 0
//
// A typedef of a multi-word type, and one as a for-loop variable.
typedef unsigned short us;
kernel void k(device uint* o [[buffer(0)]])
{
    us s = 1;
    for (unsigned int i = 0; i < 3; i++) {
        s += 1;
    }
    o[0] = s;
}
