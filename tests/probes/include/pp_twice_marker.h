// Declares a name so that including this header twice collides, which is how
// pp_include_twice_declares_twice shows the header was read twice. Both
// declarations come from the two includes: nothing is declared outside them, so
// a preprocessor that skipped the second include would leave the module with
// one declaration and the probe would fail rather than pass.
struct pp_twice_marker { int x; };