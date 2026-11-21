// EXPECT: error field "c" of "O" is a short[2], which mslc does not pass between stages yet
struct O { short c[2] [[color(0)]]; };
fragment O fragment_color_short_array_type_rejected() {
    O o;
    return o;
}
