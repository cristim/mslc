// EXPECT: valid
// DISASM: OpIEqual
// DISASM: OpINotEqual
// DISASM: OpSLessThan
// DISASM: OpSLessThanEqual
// DISASM: OpSGreaterThan
// DISASM: OpSGreaterThanEqual
// DISASM: OpULessThan
// DISASM: OpULessThanEqual
// DISASM: OpUGreaterThan
// DISASM: OpUGreaterThanEqual
// DISASM: OpFOrdEqual
// DISASM: OpFOrdNotEqual
// DISASM: OpFOrdLessThan
// DISASM: OpFOrdLessThanEqual
// DISASM: OpFOrdGreaterThan
// DISASM: OpFOrdGreaterThanEqual
// DISASM-NOT: OpFUnordLessThan
//
// Every comparison opcode, one per operator per signedness, and nothing else
// reaches them: no probe and no corpus shader used a comparison operator at all,
// so the whole of the comparison path in sema.cpp was dead until this file.
// That is the code that picks between signed and unsigned, which are different
// opcodes with different results for the same operands.
//
// The signedness pins are the point, not the coverage. `a < b` with both operands
// %int is OpSLessThan and with both %uint is OpULessThan, and the difference is
// not cosmetic: at a = -1, b = 1, the signed form says true and the unsigned form
// says false, because -1 is 4294967295. An emitter that chose the unsigned form
// for signed operands would validate, would run, and would compare wrongly, which
// is the failure this project has been hit by twice already (a float3 laid out
// as 12 bytes, and a signed widening that became OpConvertUToF).
//
// The float pins name the ordered FOrd forms rather than the unordered ones,
// because a comparison against a NaN is unordered by definition and
// OpFOrdLessThan says so. Nothing here can produce a NaN, so the pin is that
// mslc picked the ordered form and not that it handled NaN.
//
// The operands come from a buffer rather than literals, so the comparison is a
// real load and a real branch rather than something a constant folder could
// answer. `uint(a)` and `float(a)` are the MSL conversion spelling; there is no
// C-style cast in MSL, and xcrun metal rejects "(uint)a".
kernel void comparison_operators(device int *out [[buffer(0)]],
                                 constant int *in [[buffer(1)]],
                                 uint index [[thread_position_in_grid]])
{
    int a = in[index];
    int b = in[index] - 1;

    // Signed, all six.
    if (a == b) { out[0] = 1; } else { out[0] = 0; }
    if (a != b) { out[1] = 1; } else { out[1] = 0; }
    if (a < b) { out[2] = 1; } else { out[2] = 0; }
    if (a <= b) { out[3] = 1; } else { out[3] = 0; }
    if (a > b) { out[4] = 1; } else { out[4] = 0; }
    if (a >= b) { out[5] = 1; } else { out[5] = 0; }

    // Unsigned, the four whose opcode differs from the signed form.
    uint ua = uint(a);
    uint ub = uint(b);
    if (ua < ub) { out[6] = 1; } else { out[6] = 0; }
    if (ua <= ub) { out[7] = 1; } else { out[7] = 0; }
    if (ua > ub) { out[8] = 1; } else { out[8] = 0; }
    if (ua >= ub) { out[9] = 1; } else { out[9] = 0; }

    // Floating point, the ordered forms.
    float fa = float(a);
    float fb = float(b);
    if (fa == fb) { out[10] = 1; } else { out[10] = 0; }
    if (fa != fb) { out[11] = 1; } else { out[11] = 0; }
    if (fa < fb) { out[12] = 1; } else { out[12] = 0; }
    if (fa <= fb) { out[13] = 1; } else { out[13] = 0; }
    if (fa > fb) { out[14] = 1; } else { out[14] = 0; }
    if (fa >= fb) { out[15] = 1; } else { out[15] = 0; }
}
