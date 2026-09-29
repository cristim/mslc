// EXPECT: valid
// Widening that also changes signedness. The extension has to follow the value
// being extended, not the type it ends up in: spirv-val only checks the opcode
// against the result type, so extending straight to the result zero extends a
// negative short into a small positive ulong, or sign extends 65535u into a
// negative int. Both validate and both are wrong, so the value goes to a
// temporary of the source's signedness at the target width, then is
// reinterpreted. The OpBitcast needles are what the broken version lacks
// entirely; the NOT is what catches a sign extension where a zero extension
// belongs.
// DISASM: OpSConvert %long
// DISASM: OpUConvert %uint
// DISASM: OpUConvert %ulong
// DISASM: OpBitcast %ulong
// DISASM: OpBitcast %int
// DISASM: OpBitcast %long
// DISASM-NOT: OpSConvert %int
kernel void widen_across_signedness(device const short* s [[buffer(0)]],
                                    device const ushort* u [[buffer(1)]],
                                    device const uchar* b [[buffer(2)]],
                                    device ulong* uo [[buffer(3)]],
                                    device int* io [[buffer(4)]],
                                    device long* lo [[buffer(5)]],
                                    uint t [[thread_position_in_grid]])
{
    uo[t] = ulong(s[t]);
    io[t] = int(u[t]);
    lo[t] = long(b[t]);
}
