// EXPECT: error which has to be a uint or ushort scalar
// Apple: "type 'float' is not valid for attribute 'thread_index_in_threadgroup'".
//
// LocalInvocationIndex is the one compute built-in Apple takes as a SCALAR only.
// Measured with `xcrun -sdk macosx metal -c`: uint and ushort are accepted, and
// float, int, bool, half, uint2, uint3 and uint4 are all rejected.
//
// Without a guard of its own this built-in fell outside the compute-id rule,
// which admits uint and ushort up to three lanes wide for the three position
// ids. That over-accepted float, int, bool and half, and a uint3 got as far as
// the swizzle check and was reported as ".x swizzles a value that is not a
// vector", naming neither the attribute nor the actual problem.
//
// Note the vector-width convention this depends on: a scalar is vectorWidth 0,
// not 1. Reading it the other way round rejects the two forms Apple accepts.
#include <metal_stdlib>
using namespace metal;

kernel void thread_index_in_threadgroup_wrong_type(device float *d [[buffer(0)]],
                                                 float i [[thread_index_in_threadgroup]])
{
    d[0] = i;
}
