// EXPECT: error no kernel entry point in this source
// FLAG: --stage
// FLAG: kernel
//
// A library with no functions is what a source with no entry point compiles to
// when the stage is left to mslc. A caller that asks for a kernel gets an
// error instead of no module.
float scale(float x) { return x * 2.0f; }
