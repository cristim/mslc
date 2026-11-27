// EXPECT: valid
// RUN: grid=1 local=1
// DISASM-MATCH: OpTypeArray %float %uint_2
// "c[]" after a comma takes its length from its own initialiser list.
kernel void several_declarators_array_deduced_length(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  float a = 1.0, c[] = {4.0, 5.0};
  o[0] = a + c[i & 1];
}
