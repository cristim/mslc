// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
//
// Apple accepts this, since a by-value parameter is a copy: the built-in written by the body is copied into a
// local at entry, and the store goes to the copy. (This probe pinned the refusal until it was lowered.)
kernel void swizzle_store_builtin_input(device uint *out [[buffer(0)]], uint3 gid [[thread_position_in_grid]])
{
    gid.x = 1;
    out[0] = gid.x;
}
