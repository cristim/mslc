// EXPECT: error field "c" of "O" is a packed_short3, which Apple's compiler does not allow across a stage boundary
struct O { packed_short3 c [[color(0)]]; };
fragment O fragment_color_short_packed_rejected() {
    O o;
    return o;
}
