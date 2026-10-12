// EXPECT: error has the enum type Mode, and an integer does not convert to it
// Apple: "cannot initialize a variable of type Mode with an rvalue of type int". A minus
// written in the source is arithmetic, so -N1 (which is 1) is an integer.
enum Mode { N1 = -1, Z = 0, P = 5 };
kernel void enum_negated_enumerator_rejected(device int *o [[buffer(0)]]) {
  Mode m = -N1;
  o[0] = int(m);
}
