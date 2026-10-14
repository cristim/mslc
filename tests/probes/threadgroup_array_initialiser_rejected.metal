// EXPECT: error a threadgroup local "s" with an initialiser is not lowered yet
// A threadgroup array obeys the same rule as a threadgroup scalar: no
// initialiser. Apple accepts one, but how many invocations run the
// initialiser, and in what order, is not something the source states.
//
// This probe exists because the array path reaches the emitter before the
// scalar path does. It once skipped the scalar's two guards entirely, so this
// declaration compiled to a module spirv-val rejected. The rule is enforced in
// checkThreadgroupLocal, which both forms call, so they cannot drift again.
#include <metal_stdlib>
using namespace metal;

kernel void threadgroup_array_initialiser_rejected(device float *d [[buffer(0)]])
{
    threadgroup float s[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    d[0] = s[0];
}
