#include "filesystem/save_file.h"
#include <iostream>
#include <vector>

using namespace kb::kfs;

struct Header
{
    uint32_t magic;
    uint32_t version;
    uint32_t size;
};

void write_file(std::ostream& stream)
{
    Header hh;
    hh.magic = 0xdeadbeef;
    hh.version = 1;
    hh.size = 8;

    std::vector<uint32_t> data = {1, 2, 3, 4, 5, 6, 7, 8};

    stream.write(reinterpret_cast<const char*>(&hh), sizeof(hh));
    stream.write(reinterpret_cast<const char*>(&data[0]), ssize_t(sizeof(uint32_t) * data.size()));
}

void read_file(std::istream& stream)
{
    Header hh;
    stream.read(reinterpret_cast<char*>(&hh), sizeof(hh));

    std::vector<uint32_t> data(hh.size);
    stream.read(reinterpret_cast<char*>(&data[0]), ssize_t(sizeof(uint32_t) * data.size()));

    std::cout << "Magic: " << std::hex << hh.magic << std::endl;
    std::cout << "Version: " << hh.version << std::endl;
    std::cout << "Size: " << hh.size << std::endl;

    for (size_t ii = 0; ii < data.size(); ++ii)
    {
        std::cout << data[ii] << ' ';
    }
    std::cout << std::endl;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    SaveFile sf("save.dat");
    write_file(*sf);

    auto tmp_path = sf.get_temporary_path();
    std::cout << "Temporary file: " << tmp_path << std::endl;

    sf.commit();

    std::ifstream ifs("save.dat");
    read_file(ifs);
}