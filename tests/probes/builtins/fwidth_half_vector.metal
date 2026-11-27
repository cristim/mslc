// EXPECT: valid
// DISASM: %42 = OpFConvert %v2float %41
// DISASM: %44 = OpDPdx %v2float %42
// DISASM: %45 = OpFConvert %v2half %44
// DISASM: %46 = OpExtInst %v2half %43 FAbs %45
// DISASM: %47 = OpDPdy %v2float %42
// DISASM: %48 = OpFConvert %v2half %47
// DISASM: %49 = OpExtInst %v2half %43 FAbs %48
// DISASM: %50 = OpFAdd %v2half %46 %49
// DISASM-ORDER: %42 = OpFConvert %v2float %41
// DISASM-ORDER: %50 = OpFAdd %v2half %46 %49
// Lowering: half -> float32 OpDPdx and OpDPdy, each narrowed to half, FAbs, half OpFAdd (matches Apple, #173).
// The exact SSA ids are pinned deliberately: a generic substring also matches
// an unrelated conversion in this fixture family (caught during review for
// the scalar probe). Do not loosen these pins to a bare substring without
// re-checking against a real disassembly first.
struct In { float4 position [[position]]; half2 v [[user(locn0)]]; };
fragment float4 main(In p [[stage_in]]) { half2 width = fwidth(p.v); return float4(float(width.x)); }
