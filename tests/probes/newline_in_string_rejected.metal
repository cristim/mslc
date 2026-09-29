// EXPECT: error newline in a string literal
// A raw newline in a string literal is not legal C++ and Apple's compiler
// rejects it, but mslc used to accept it and emit a module. Worse, the token
// swallowed the line, so the line count after it was too low and the
// end-of-line bound that ends a directive landed in the wrong place. Reporting
// it matches the oracle and keeps the line count meaningful.
#define A "x
y"
kernel void newline_in_string_rejected(device uint* out [[buffer(0)]],
                                       uint i [[thread_position_in_grid]])
{ out[i] = i; }
