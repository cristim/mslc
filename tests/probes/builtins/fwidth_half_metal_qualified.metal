// EXPECT: valid
// DISASM: %39 = OpFConvert %float %38
// DISASM: %41 = OpDPdx %float %39
// DISASM: %42 = OpFConvert %half %41
// DISASM: %43 = OpExtInst %half %40 FAbs %42
// DISASM: %44 = OpDPdy %float %39
// DISASM: %45 = OpFConvert %half %44
// DISASM: %46 = OpExtInst %half %40 FAbs %45
// DISASM: %47 = OpFAdd %half %43 %46
// DISASM-ORDER: %39 = OpFConvert %float %38
// DISASM-ORDER: %47 = OpFAdd %half %43 %46
// Lowering: half -> float32 OpDPdx and OpDPdy, each narrowed to half, FAbs, half OpFAdd (matches Apple, #173).
// The exact SSA ids are pinned deliberately: a generic substring also matches
// an unrelated conversion in this fixture family (caught during review for
// the scalar probe). Do not loosen these pins to a bare substring without
// re-checking against a real disassembly first.
struct In { float4 position [[position]]; half v [[user(locn0)]]; };
fragment float4 main(In p [[stage_in]]) { return float4(float(metal::fwidth(p.v))); }
