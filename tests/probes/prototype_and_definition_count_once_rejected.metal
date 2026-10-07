// EXPECT: error declares 1 helper function and no kernel, vertex or fragment entry point
//
// A prototype and its definition are one function; the count is by name, so
// this reports 1, not 2.
float scale(float x);
float scale(float x) { return x * 2.0f; }
