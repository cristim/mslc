// EXPECT: valid
// DISASM: %40 = OpFConvert %v4float %39
// DISASM: %41 = OpFwidth %v4float %40
// DISASM: %42 = OpFConvert %v4half %41
// DISASM-ORDER: %40 = OpFConvert %v4float %39
// DISASM-ORDER: %42 = OpFConvert %v4half %41
// The exact SSA ids are pinned deliberately: a generic substring also matches
// an unrelated conversion in this fixture family (caught during review for
// the scalar probe). Do not loosen these pins to a bare substring without
// re-checking against a real disassembly first.
// Pins mslc's lowering (half -> float32 OpFwidth -> half). Parity with Apple's native air.fwidth.f16 is unmeasured; see #173.
struct In { float4 position [[position]]; half4 v [[user(locn0)]]; };
fragment float4 main(In p [[stage_in]]) { half4 width = fwidth(p.v); return float4(float(width.x)); }
