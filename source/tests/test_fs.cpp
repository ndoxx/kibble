#include "filesystem/filesystem.h"
#include "filesystem/resource_pack.h"
#include <catch2/catch_all.hpp>
#include <iostream>
#include <numeric>

using namespace kb;

class PathFixture
{
public:
    PathFixture() { filesystem.add_directory_alias(filesystem.get_self_directory() / "../../data", "data"); }

protected:
    kfs::FileSystem filesystem;
};

TEST_CASE_METHOD(PathFixture, "Getting self directory", "[path]")
{
    REQUIRE(fs::exists(filesystem.get_self_directory() / "test_kibble"));
}

TEST_CASE_METHOD(PathFixture, "Retrieving aliased directory", "[path]")
{
    const auto& dir = filesystem.get_aliased_directory("data"_h);
    REQUIRE(fs::equivalent(dir, filesystem.get_self_directory() / "../../data"));
}

TEST_CASE_METHOD(PathFixture, "Retrieving file path using a universal path string", "[path]")
{
    auto client_cfg_filepath = filesystem.universal_path("data://config/client.toml");
    const auto& dir = filesystem.get_aliased_directory("data"_h);
    REQUIRE(fs::equivalent(client_cfg_filepath, dir / "config/client.toml"));
}

TEST_CASE_METHOD(PathFixture, "Making a universal path string from a path and a directory alias", "[path]")
{
    auto client_cfg_filepath = filesystem.universal_path("data://config/client.toml");
    auto upath = filesystem.make_universal(client_cfg_filepath, "data"_h);
    REQUIRE(!upath.compare("data://config/client.toml"));
}

class ReadWriteFixture
{
public:
    ReadWriteFixture() : data(256)
    {
        filesystem.add_directory_alias(filesystem.get_self_directory() / "../../data", "data");
        std::iota(data.begin(), data.end(), 0);
        std::ofstream ofs(filesystem.universal_path("data://iotest/data.dat"), std::ios::binary);
        ofs.write(reinterpret_cast<char*>(data.data()), long(data.size() * sizeof(char)));
    }

protected:
    kfs::FileSystem filesystem;
    std::vector<char> data;
};

TEST_CASE_METHOD(ReadWriteFixture, "Getting a file as a std::vector<char>", "[rw]")
{
    auto data_vec = filesystem.get_file_as_vector<char>("data://iotest/data.dat");
    REQUIRE(std::equal(data_vec.begin(), data_vec.end(), data.begin()));
}

class KpakFixture
{
public:
    KpakFixture():
    expected_data(256)
    {
        // Create temporary directory with data
        fs::create_directory("/tmp/kibble_test");
        fs::create_directory("/tmp/kibble_test/resources");
        fs::create_directory("/tmp/kibble_test/resources/textures");
        filesystem.add_directory_alias("/tmp/kibble_test", "test");

        std::iota(expected_data.begin(), expected_data.end(), 0);
        expected_text =
            R"(The BBC Micro could utilise the Teletext 7-bit character set, which had 128 box-drawing characters, 
            whose code points were shared with the regular alphanumeric and punctuation characters. Control 
            characters were used to switch between regular text and box drawing.[4]
            The BBC Master and later Acorn computers have the soft font by default defined with line drawing characters.
            )";

        std::ofstream ofs("/tmp/kibble_test/resources/textures/tex1.dat");
        ofs.write(expected_data.data(), long(expected_data.size()));
        ofs.close();
        ofs.open("/tmp/kibble_test/resources/textures/tex2.dat");
        ofs.write(expected_data.data(), long(expected_data.size()));
        ofs.close();
        ofs.open("/tmp/kibble_test/resources/text_file.txt");
        ofs.write(expected_text.data(), long(expected_text.size()));
    }

    ~KpakFixture() { fs::remove_all("/tmp/kibble_test"); }

protected:
    kfs::FileSystem filesystem;
    std::string expected_text;
    std::vector<char> expected_data;
};

TEST_CASE_METHOD(KpakFixture, "Retrieving data from pack", "[kpak]")
{
    kfs::pack_directory(filesystem.universal_path("test://resources"), 
                        filesystem.universal_path("test://resources.kpak"));

    REQUIRE(fs::exists(filesystem.universal_path("test://resources.kpak")));
    
    kfs::PackFile pack(filesystem.universal_path("test://resources.kpak"));
    std::ifstream ifs(filesystem.universal_path("test://resources.kpak"), std::ios::binary);
    
    {
        const auto& text_file_entry = pack.get_entry("text_file.txt");
        std::string retrieved;
        retrieved.resize(text_file_entry.size);
        ifs.seekg(text_file_entry.offset);
        ifs.read(retrieved.data(), text_file_entry.size);
        REQUIRE(!retrieved.compare(expected_text));
    }

    {
        const auto& texture1_file_entry = pack.get_entry("textures/tex1.dat");
        std::vector<char> retrieved(texture1_file_entry.size);
        ifs.seekg(texture1_file_entry.offset);
        ifs.read(retrieved.data(), texture1_file_entry.size);
        REQUIRE(retrieved == expected_data);
    }

    {
        const auto& texture2_file_entry = pack.get_entry("textures/tex2.dat");
        std::vector<char> retrieved(texture2_file_entry.size);
        ifs.seekg(texture2_file_entry.offset);
        ifs.read(retrieved.data(), texture2_file_entry.size);
        REQUIRE(retrieved == expected_data);
    }

    ifs.close();
}