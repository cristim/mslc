// EXPECT: valid
// DISASM: OpConvertUToF %float %uint
// DISASM: OpCompositeConstruct %v3float
//
// Direct initialisation: "float3 zero(0);" names the type, then the variable,
// then the value to build it from. The parser read the name and then hit "("
// where it wanted ";" and reported "expected ; after a declaration, found (",
// which is where test/lighting in the indium corpus is stuck on the line
// "float3 specularTerm(0);".
//
// The 0 is an integer literal and the components are floats, so the value is
// converted before it goes in: OpConvertUToF, then the same value in all three
// components. The conversion is pinned rather than left to the module being
// valid because reinterpreting the int as a float instead is a silently wrong
// shader, not a rejected one.
//
// OpCompositeConstruct %v3float with that one value as all three operands is a
// broadcast, which is what float3(0) means. Converting a scalar to the vector's
// type instead is the mistake this replaces, and the grammar forbids it:
// OpFConvert requires the result and the operand to have the same component
// count, so it cannot spell a scalar to vector at all.
//
// Two limits of these pins, both real. The claim that makes it a broadcast is
// that the three operands are the same id, and CMake's regex has no
// backreferences, so the harness cannot express "the same capture twice"; the
// line in the disassembly is "%49 = OpCompositeConstruct %v3float %48 %48 %48".
// And 0 is the one literal for which the int to float conversion is
// indistinguishable from a reinterpretation, so this probe says nothing about
// a literal whose value distinguishes them: "float3 v(-1)" broadcasts
// 4294967295.0f, because an int literal is emitted as %uint and OpSNegate of a
// %uint wraps. That one predates this change and is tracked separately.
kernel void direct_init_broadcast(device float3 *out [[buffer(0)]],
                                  constant float3 *in [[buffer(1)]],
                                  uint index [[thread_position_in_grid]])
{
    float3 zero(0);
    out[index] = zero + in[index];
}
