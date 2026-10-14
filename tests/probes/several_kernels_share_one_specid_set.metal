// EXPECT: valid
// Every kernel in a module carries a LocalSizeId naming three ids.
//
// This probe does NOT pin that those three ids are the same across kernels,
// which is the property that matters. The probe runner's needles are substring
// matches, so two identical patterns both match the first instruction and a
// module with three kernels would satisfy both from one of them, and no needle
// available here can compare two instructions' operands. tools/check_ids_specid_test.py
// is what checks that property, by counting SpecId decorations; run it, don't
// read this file as evidence for it.
//
// What this file does say is that a module with several kernels names a
// workgroup size for each of them.
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