// EXPECT: error field "c" of "O" is a short8, which mslc does not pass between stages yet
struct O { short8 c [[color(0)]]; };
fragment O fragment_color_short_wide_vector_rejected() {
    O o;
    return o;
}
