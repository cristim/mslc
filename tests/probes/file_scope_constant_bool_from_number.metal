// EXPECT: error a bool constant is initialised with true or false
//
// A bool has no literal form in SPIR-V: OpConstant takes a word and the
// grammar allows none for OpTypeBool, so there is only OpConstantTrue and
// OpConstantFalse. Folding a number into a bool would emit a word where the
// module allows none, which spirv-val rejects; reporting it names the mistake.
constant bool kEnabled = 1;

kernel void file_scope_constant_bool_from_number(device float* out [[buffer(0)]],
                                                 uint index [[thread_position_in_grid]])
{
    out[index] = 1.0;
}
