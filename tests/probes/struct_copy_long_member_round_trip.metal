// EXPECT: valid
// DISASM-NO-MATCH: OpBitcast %_struct
// DISASM-MATCH: OpLoad %_struct_[0-9]+ %[0-9]+ Aligned 8
//
// A whole-struct access is aligned as its largest member, so a long makes it 8,
// which spirv-val requires to be at least the largest scalar. Read back on
// lavapipe and on an Apple GPU the stored members match; the padding between
// the members is not written.
struct L {
    long a;
    float b;
    ulong c;
};

kernel void struct_copy_long_member_round_trip(device L* out [[buffer(0)]],
                                               const device L* in [[buffer(1)]])
{
    L z = in[1];
    z.b = z.b + 100.0;
    out[0] = z;
    L y = in[0];
    out[1] = y;
}
