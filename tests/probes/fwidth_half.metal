// EXPECT: valid
// DISASM: %39 = OpFConvert %float %38
// DISASM: %40 = OpFwidth %float %39
// DISASM: %41 = OpFConvert %half %40
// DISASM-ORDER: %39 = OpFConvert %float %38
// DISASM-ORDER: %41 = OpFConvert %half %40
// The exact SSA ids are pinned deliberately: a generic substring also matches
// an unrelated conversion in this fixture (caught during review). Do not
// loosen these pins to a bare substring without re-checking against a real
// disassembly first.
// Pins mslc's lowering (half -> float32 OpFwidth -> half). Parity with Apple's native air.fwidth.f16 is unmeasured; see #173.
struct In { float4 position [[position]]; half v [[user(locn0)]]; };
fragment float4 main(In p [[stage_in]]) { return float4(float(fwidth(p.v) + p.v)); }
