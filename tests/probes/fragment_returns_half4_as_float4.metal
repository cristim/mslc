// EXPECT: valid
// DISASM: = OpVariable %_ptr_Output_v4float Output
// DISASM-MATCH: OpDecorate %[0-9]+ Location 0
// DISASM-NOT: OpTypePointer Output %v4half
// DISASM: OpFConvert %v4float
// DISASM-NOT: OpReturnValue
// DISASM-MATCH: OpEntryPoint Fragment %[0-9]+ "fragment_returns_half4_as_float4" %[0-9]+
//
// A fragment function's returned colour is an Output at Location 0. A half4 is
// written as a float4: Vulkan's shaderFloat16 does not extend to Input and
// Output, which need storageInputOutput16, and widening a half is exact, so the
// colour that reaches the attachment is the half value.
fragment half4 fragment_returns_half4_as_float4()
{
    return half4(0.5);
}
