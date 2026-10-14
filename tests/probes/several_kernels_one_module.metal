// EXPECT: valid
// DISASM-MATCH: OpEntryPoint GLCompute %[0-9]+ "b"
// DISASM-MATCH: OpEntryPoint GLCompute %[0-9]+ "a"
//
// Two kernels in one module. They share the module's one workgroup size,
// which is what makes this legal: indium writes constantID 0, 1, 2 once per
// pipeline against the module it was handed, so a module has one size, and
// [[threads_per_threadgroup]] reads that same size for both.
//
// Apple's compiler accepts this, and mslc refused it with "a source may
// declare one kernel". Both entry points must survive into the module, which
// the two needles above check by name.
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