// EXPECT: error constructing a float* is not lowered yet
//
// Direct initialisation reaches a type that is not a value. "float* p(0)" is a
// null pointer in C++, not a pointer built out of 0, and mslc has no null
// pointer constant to put in it.
//
// The pin is the diagnostic because the module this used to produce validates.
// A local of a pointer type is declared as a pointer to the pointee's value
// type, and the initialiser goes through the same conversion as any other, so
// the 0 became OpConvertUToF %float %uint_0_0 and was then stored into a
// %_ptr_Function_float variable: a float 0.0 written where a pointer belongs.
// spirv-val accepts that. Nothing downstream would report it either, which is
// why this is a pin and not a comment.
//
// The "=" spelling, "float* p = 0;", reaches the same wrong module on master and
// is tracked separately; what this probe holds is that the new syntax does not
// add a second route to it.
kernel void construct_a_pointer_not_lowered(device float *out [[buffer(0)]],
                                             uint index [[thread_position_in_grid]])
{
    device float *p(0);
    out[index] = 1.0;
}
