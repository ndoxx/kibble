#include "kibble/filesystem/md5.h"

#include "fmt/format.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::string arg = argv[1];
    kb::md5 md;
    md.process(arg.data(), arg.size());
    md.finish();
    fmt::print("{}\n", md.to_string());

    return 0;
}