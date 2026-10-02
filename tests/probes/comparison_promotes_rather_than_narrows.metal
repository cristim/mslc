// EXPECT: valid
// DISASM: OpConvertSToF
// DISASM: OpFOrdLessThan
// DISASM-NOT: OpConvertFToS
//
// A comparison whose two operands have different widths, which C resolves by
// promoting the narrower one rather than by narrowing the wider one.
//
// The rule the emitter used was the opposite: it converted the right operand to
// the left one's type. That is wrong in both directions and both validate.
//
//   float f; if (3 < f)      the 3 became an int, so the answer was false where
//                            MSL says true. On master this was not a wrong
//                            answer but an invalid module, spirv-val rejecting
//                            OpSLessThan's float operands, so the branch traded
//                            a loud failure for a silent one.
//
//   uchar b; int v; if (b < v)   the int became a uchar, so 260 wrapped to 4
//                            and "5 < 260" was false.
//
//   uint u; long s; if (u > s)   the long became a uint, so -1 became
//                            4294967295 and "5 > -1" was false.
//
// The pins are the float case alone, and that is on purpose. It is the one whose
// answer is unambiguous in the disassembly: the integer literal is converted up
// to float and compared with OpFOrdLessThan, where narrowing produced
// OpConvertFToS and a signed comparison over a float operand.
//
// The integer cases are here to be compiled and to exercise the promotion, not to
// be pinned by opcode. Promoting a uchar against an int gives OpSLessOver, which
// is correct and is what the narrowing destroyed, so a DISASM-NOT for the signed
// comparison would fail on a right answer. Pinning them by value needs a
// read-back harness, which this project keeps in scratch rather than here.
kernel void comparison_promotes_rather_than_narrows(
    device float *out [[buffer(0)]],
    constant float *fIn [[buffer(1)]],
    constant uint *uIn [[buffer(2)]],
    uint index [[thread_position_in_grid]])
{
    float f = fIn[index];
    if (3 < f) { out[0] = 1.0; } else { out[0] = 0.0; }

    uchar b = uchar(5);
    int v = 260;
    if (b < v) { out[1] = 1.0; } else { out[1] = 0.0; }

    uint u = uIn[index];
    long s = -1;
    if (u > s) { out[2] = 1.0; } else { out[2] = 0.0; }
}
