// EXPECT: no module
// REFLECT-NOT: "name"
//
// A prototype and its definition are one function, and with no entry point the
// source is a library with no module.
float scale(float x);
float scale(float x) { return x * 2.0f; }
