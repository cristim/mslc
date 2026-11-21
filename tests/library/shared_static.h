// A header every unit includes: an internal helper and the declaration of an
// external one. The internal helper must not be exported by each includer.
#ifndef SHARED_STATIC_H
#define SHARED_STATIC_H
static inline float sq(float x) { return x * x; }
float ext(float value);
#endif
