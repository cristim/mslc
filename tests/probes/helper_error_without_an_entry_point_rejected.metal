// EXPECT: error "undeclared" is not a parameter, local, constant or builtin
//
// A source with no entry point has no module, but it is compiled all the same:
// an error in a helper fails the library, as it does in Apple's compiler.
float scale(float x) { return x * undeclared; }
