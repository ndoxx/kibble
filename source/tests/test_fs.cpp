#include "filesystem/filesystem.h"
#include <catch2/catch_all.hpp>
#include <numeric>
#include <iostream>

using namespace kb;

class PathFixture
{
public:
    PathFixture()
    {
    	filesystem.add_directory_alias(filesystem.get_self_directory() / "../../data", "data");
    }

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
    ReadWriteFixture():
    data(256)
    {
    	filesystem.add_directory_alias(filesystem.get_self_directory() / "../../data", "data");
	    std::iota(data.begin(), data.end(), 0);
	    std::ofstream ofs(filesystem.universal_path("data://iotest/data.dat"), std::ios::binary);
        ofs.write(reinterpret_cast<char*>(data.data()), long(data.size()*sizeof(char)));
    }

protected:
    kfs::FileSystem filesystem;
    std::vector<char> data;
};

TEST_CASE_METHOD(ReadWriteFixture, "Getting a file as a std::vector<char>", "[path]")
{
    auto data_vec = filesystem.get_file_as_vector<char>("data://iotest/data.dat");
    REQUIRE(std::equal(data_vec.begin(), data_vec.end(), data.begin()));
}
