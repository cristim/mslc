// EXPECT: valid
// DISASM: OpTypeArray %_arr_float_uint_3 %uint_2
// DISASM: OpTypePointer Workgroup
//
// A two-dimensional threadgroup array indexes through more than one dimension,
// which takes a different walk from a one-dimensional one: the outer index
// steps the array type down, the inner index steps the row. That walk asks
// whether a binding names a variable this compiler declares on its own, and
// the answer is a list of storage classes. Workgroup was missing from it, so a
// multi-dimensional threadgroup array was reported as "only an element of a
// vector is lowered here", which names a vector that is not in the source.
//
// isLocalStorage is now the single place that list lives, so an array path and
// a threadgroup variable cannot disagree about it again. This probe fails if
// Workgroup is dropped from that list, which is the regression it exists for.
#include <metal_stdlib>
using namespace metal;

kernel void threadgroup_array_two_dimensional(device float *d [[buffer(0)]],
                                             uint t [[thread_position_in_threadgroup]])
{
  threadgroup float z[2][3];
  threadgroup_barrier(mem_flags::mem_threadgroup);
  z[1][t % 3] = (float)t;
  d[0] = z[0][1];
  d[1] = z[1][0];
}
