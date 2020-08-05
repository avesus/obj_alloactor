#define RECOMPUTE_SYS_INFO_VALUES  // so that will re-read values
#include <system/sys_info.h>
#include <util/arg.h>

char * file_path = NULL;

int
main(int argc, char ** argv) {
    PREPARE_PARSER;
    // clang-format off
    ADD_ARG("-f", "--file", false, String, file_path, "Set file to write sys info header (this is mostly for default)");
    // clang-format on
    PARSE_ARGUMENTS;
    sysi::create_defs(file_path);
}
