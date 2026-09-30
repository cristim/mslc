// EXPECT: error builtin "thread_position_in_grid" is not available in a fragment function
// A dispatch id in a fragment function. MSL rejects it, and so does Vulkan:
// GlobalInvocationId is a GLCompute builtin, and an integer Input on a fragment
// entry point additionally has to carry Flat or validation fails. Emitting a
// module for it would mean a shader that validates against one stage's rules
// and computes nothing like what the source asked for.
//
// This is a second thing several entry points made reachable: with one entry
// point per module the mismatch was unreachable, because a fragment function
// was the only thing that could reach the fragment path.
fragment void wrong_stage_builtin(device float* out [[buffer(0)]],
                                  uint index [[thread_position_in_grid]])
{
    out[index] = 1.0;
}
