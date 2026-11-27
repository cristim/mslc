// EXPECT: valid
// DISASM: %53 = OpFConvert %float %52
// DISASM: %55 = OpDPdx %float %53
// DISASM: %56 = OpFConvert %half %55
// DISASM: %57 = OpExtInst %half %54 FAbs %56
// DISASM: %58 = OpDPdy %float %53
// DISASM: %59 = OpFConvert %half %58
// DISASM: %60 = OpExtInst %half %54 FAbs %59
// DISASM: %61 = OpFAdd %half %57 %60
// DISASM-ORDER: %53 = OpFConvert %float %52
// DISASM-ORDER: %61 = OpFAdd %half %57 %60
// Lowering: half -> float32 OpDPdx and OpDPdy, each narrowed to half, FAbs, half OpFAdd (matches Apple, #173).
// The exact SSA ids are pinned deliberately: a generic substring also matches
// an unrelated conversion in this fixture (caught during review). Do not
// loosen these pins to a bare substring without re-checking against a real
// disassembly first.
struct In { float4 position [[position]]; half v [[user(locn0)]]; };
half footprint(half value) { return fwidth(value) + value; }
fragment float4 main(In p [[stage_in]]) { return float4(float(footprint(p.v))); }
