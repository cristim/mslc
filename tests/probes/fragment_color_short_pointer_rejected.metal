// EXPECT: error field "c" of "O" is a short*, which mslc does not pass between stages yet
struct O { device short* c [[color(0)]]; };
fragment O fragment_color_short_pointer_rejected() {
    O o;
    return o;
}
