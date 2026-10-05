// EXPECT: error assigning to ".x" of a value that is not a local, a struct member or a buffer element
//
// Apple accepts this, since a by-value parameter is a copy; mslc binds the
// built-in to its read-only Input variable and has no copy to store into.
kernel void swizzle_store_builtin_input_rejected(device uint *out [[buffer(0)]], uint3 gid [[thread_position_in_grid]])
{
    gid.x = 1;
    out[0] = gid.x;
}
