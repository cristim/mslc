// EXPECT: valid
// The Aligned operand is the alignment the buffer's own layout guarantees, so
// it tracks the element's own size: 1 for a char, 2 for a short, 4 for a float
// and 8 for a long. Claiming more than the layout gives is what lets a compiler
// read past the end of a buffer, so the narrow cases are pinned too.
// DISASM: Aligned 1
// DISASM: Aligned 2
// DISASM: Aligned 4
// DISASM: Aligned 8
// DISASM-NO-MATCH: Aligned 16
// DISASM-NO-MATCH: Aligned 32
kernel void buffer_alignment_is_the_minimum(device const uchar* a [[buffer(0)]],
                                           device const ushort* b [[buffer(1)]],
                                           device const ulong* c [[buffer(2)]],
                                           device float* out [[buffer(3)]],
                                           uint i [[thread_position_in_grid]])
{ out[i] = float(a[i]) + float(b[i]) + float(c[i]); }
