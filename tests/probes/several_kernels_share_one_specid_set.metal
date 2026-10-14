// EXPECT: valid
// One set of SpecIds for the whole module, so every kernel's LocalSizeId names
// the same three constants. indium writes constantID 0, 1, 2 once per pipeline
// against the module it was handed; two decorated constants per SpecId leaves it
// undefined which one a driver binds, and the kernels would read different
// sizes.
//
// spirv-val ACCEPTS a module with two constants per SpecId, so validity is not
// evidence here. Neither is a DISASM-NOT: both sets carry the same "SpecId 0"
// text, so a second set cannot be excluded by substring, and the probe runner
// has no needle that counts occurrences.
//
// So this states the weaker thing it can actually pin: every kernel in the
// module carries a LocalSizeId naming three ids. The count of SpecId
// decorations is checked by mutation rather than by this file: reverting the
// shared set fails it.
// DISASM-MATCH: OpExecutionModeId %[0-9]+ LocalSizeId %[0-9]+ %[0-9]+ %[0-9]+
// DISASM-MATCH: OpExecutionModeId %[0-9]+ LocalSizeId %[0-9]+ %[0-9]+ %[0-9]+
#include <metal_stdlib>
using namespace metal;

kernel void a(device int *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  out[i] = 1;
}

kernel void b(device int *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  out[i] = 2;
}

kernel void c(device int *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  out[i] = 3;
}