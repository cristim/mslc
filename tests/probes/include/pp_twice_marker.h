// Declares a name so that including this header twice collides, which is how
// pp_include_twice_declares_twice shows that the header was read twice.
struct pp_twice_marker { int x; };
