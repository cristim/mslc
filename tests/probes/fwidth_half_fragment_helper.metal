// EXPECT: valid
// DISASM: %53 = OpFConvert %float %52
// DISASM: %54 = OpFwidth %float %53
// DISASM: %55 = OpFConvert %half %54
// DISASM-ORDER: %53 = OpFConvert %float %52
// DISASM-ORDER: %55 = OpFConvert %half %54
// The exact SSA ids are pinned deliberately: a generic substring also matches
// an unrelated conversion in this fixture (caught during review). Do not
// loosen these pins to a bare substring without re-checking against a real
// disassembly first.
// Pins mslc's lowering (half -> float32 OpFwidth -> half). Parity with Apple's native air.fwidth.f16 is unmeasured; see #173.
struct In { float4 position [[position]]; half v [[user(locn0)]]; };
half footprint(half value) { return fwidth(value) + value; }
fragment float4 main(In p [[stage_in]]) { return float4(float(footprint(p.v))); }
