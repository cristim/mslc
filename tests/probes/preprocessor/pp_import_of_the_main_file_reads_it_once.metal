// EXPECT: valid
// DISASM: "pp_import_main"
// Apple: the main file counts as read, so an #import of it reads nothing. Reading it
// again would redefine the struct.
struct PpImportMain { int a; };
#import "pp_import_of_the_main_file_reads_it_once.metal"

kernel void pp_import_main(device int* out [[buffer(0)]])
{
    PpImportMain s;
    s.a = 4;
    out[0] = s.a;
}
