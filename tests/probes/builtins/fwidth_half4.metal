// EXPECT: valid
// DISASM: %40 = OpFConvert %v4float %39
// DISASM: %42 = OpDPdx %v4float %40
// DISASM: %43 = OpFConvert %v4half %42
// DISASM: %44 = OpExtInst %v4half %41 FAbs %43
// DISASM: %45 = OpDPdy %v4float %40
// DISASM: %46 = OpFConvert %v4half %45
// DISASM: %47 = OpExtInst %v4half %41 FAbs %46
// DISASM: %48 = OpFAdd %v4half %44 %47
// DISASM-ORDER: %40 = OpFConvert %v4float %39
// DISASM-ORDER: %48 = OpFAdd %v4half %44 %47
// Lowering: half -> float32 OpDPdx and OpDPdy, each narrowed to half, FAbs, half OpFAdd (matches Apple, #173).
// The exact SSA ids are pinned deliberately: a generic substring also matches
// an unrelated conversion in this fixture family (caught during review for
// the scalar probe). Do not loosen these pins to a bare substring without
// re-checking against a real disassembly first.
struct In { float4 position [[position]]; half4 v [[user(locn0)]]; };
fragment float4 main(In p [[stage_in]]) { half4 width = fwidth(p.v); return float4(float(width.x)); }
