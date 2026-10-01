// EXPECT: error a local of array type float[2] is not lowered yet
//
// xcrun metal rejects "float[2] v;" ("brackets are not allowed here") and
// accepts "float v[2];", which mslc does not parse yet. So this pins the form
// that is not legal MSL and that mslc used to accept, since a module is what
// matters and the wrong one was emitted for it.
//
// A local of an array type was declared as its element, because a function that
// answers "what type is this" with a single value has no answer for an array but
// the element, and the declaration then stored into that. The module had no array
// type in it at all, no runtime array and no length. "float[2] v;" emitted 816
// bytes, spirv-val accepted them, and any read of v[0] and v[1] through the
// declaration mslc built was reading a value the source never wrote.
//
// The same blindness is #34's, where a pointer type was declared as its pointee
// and a converted scalar stored into it, and both are one defect: this function
// answers a value type, and neither an array nor a pointer is a value. It lives
// in the local path rather than in declaredTypeOf because a buffer parameter is a
// pointer by definition, and its pointee is what a member of the binding-0 block
// has to point at.
kernel void array_local_not_lowered(device float *out [[buffer(0)]],
                                    uint index [[thread_position_in_grid]])
{
    float[2] v;
    out[index] = 1.0;
}
