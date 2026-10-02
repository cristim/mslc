// EXPECT: valid
// DISASM: OpSNegate %int
// DISASM: OpConvertSToF %float
// DISASM: OpCompositeConstruct %v3float
// DISASM-NOT: OpSNegate %uint
// DISASM-NOT: OpConvertUToF
//
// A negative int literal, which is the one place the literal's signedness is
// observable in the value rather than in the spelling.
//
// An int literal used to be emitted as a %uint, so "-1" was OpSNegate %uint
// %uint_1, and negating 1 as an unsigned wraps to 4294967295. Two things then
// went differently. "int b = -1" survived, because a declaration stores the
// initialiser through a bitcast back to %int, so the wrapped pattern was read as
// -1 again. "float3 v(-1)" did not: the broadcast converts the wrapped unsigned
// straight to float, so every component was 4294967295.0f instead of -1.0f.
//
// That is why the unsigned opcode is pinned absent rather than merely unused. At
// 0 the two are indistinguishable, since 0 and -0 have the same bit pattern, so a
// probe using 0 cannot tell a correct emitter from the broken one. A signed
// negation and a signed conversion are what a signed literal is, and both are
// absent from the module otherwise.
//
// The same value reached through a declaration, so the two spellings of -1 are
// both here and both have to agree. They did not before this: one read -1.0 and
// the other 4294967295.0, in one module, from one source. Nothing reported it:
// spirv-val passes either, and a shader that returns 4294967295.0 instead of -1.0
// is a shader that runs.
kernel void negative_int_literal(device float3 *out [[buffer(0)]],
                                 device float *scalarOut [[buffer(1)]],
                                 uint index [[thread_position_in_grid]])
{
    // The broadcast, which was the broken one.
    float3 broadcast(-1);
    out[index] = broadcast;

    // The same value through a declaration, which was the one that hid it.
    int declared = -1;
    scalarOut[0] = float(declared);

    // And a negation of something loaded, so the same opcode is reached without a
    // literal in it.
    float loaded = float(declared);
    scalarOut[1] = 0.0 - loaded;
}
