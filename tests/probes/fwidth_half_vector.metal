// EXPECT: valid
// DISASM: %42 = OpFConvert %v2float %41
// DISASM: %43 = OpFwidth %v2float %42
// DISASM: %44 = OpFConvert %v2half %43
// DISASM-ORDER: %42 = OpFConvert %v2float %41
// DISASM-ORDER: %44 = OpFConvert %v2half %43
// The exact SSA ids are pinned deliberately: a generic substring also matches
// an unrelated conversion in this fixture family (caught during review for
// the scalar probe). Do not loosen these pins to a bare substring without
// re-checking against a real disassembly first.
// Pins mslc's lowering (half -> float32 OpFwidth -> half). Parity with Apple's native air.fwidth.f16 is unmeasured; see #173.
struct In { float4 position [[position]]; half2 v [[user(locn0)]]; };
fragment float4 main(In p [[stage_in]]) { half2 width = fwidth(p.v); return float4(float(width.x)); }
