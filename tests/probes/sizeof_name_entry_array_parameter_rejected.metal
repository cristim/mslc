// EXPECT: error worked out after parsing
// Apple gives 8 for sizeof of an array parameter, which decays to a pointer. The entry point
// keeps its array parameter undecayed, so its type is not recorded and the size is refused,
// not given as the 4 of one element.
kernel void sizeof_name_entry_array_parameter_rejected(device float out[])
{
    float a[sizeof(out)];
    out[0] = a[0];
}
