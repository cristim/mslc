// EXPECT: no module
// REFLECT: "entry_points": [
// REFLECT-NOT: "name"
//
// The case a Metal shader library is actually written in: helper functions and
// no entry point. Apple's compiler builds a library of them that lists no
// function, so this is a library with no module.
float scale(float x) { return x * 2.0f; }
