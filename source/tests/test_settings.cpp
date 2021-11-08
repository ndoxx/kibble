#include "config/config.h"
#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <string_view>
namespace fs = std::filesystem;

constexpr std::string_view cfg_source = R"(
my_string = "Hello"
my_float = 1.42
my_int = -42
my_uint = 42
my_bool = true
my_other_bool = false

[logger]
    [[logger.channels]]
        name = "application"
        verbosity = 3
    [[logger.channels]]
        name = "editor"
        verbosity = 3
    [[logger.channels]]
        name = "event"
        verbosity = 1
    [[logger.sinks]]
        type = "ConsoleSink"
        channels = "all"
    [[logger.sinks]]
        type = "MainFileSink"
        channels = "all"
        destination = "erwin.log"

[renderer]
    backend = "OpenGL"
    max_2d_batch_count = 8192
    enable_cubemap_seamless = true

[memory]
    renderer_area_size = 32
    system_area_size = 1
    [memory.renderer]
        queue_buffer_size = 1
        pre_buffer_size = 1
        post_buffer_size = 1
        auxiliary_arena_size = 20
)";

class SettingsFixture
{
public:
    SettingsFixture()
    {
        // Write to tmp file
        std::ofstream file("tmp.toml");
        file << cfg_source << std::endl;
        file.close();
    }

    ~SettingsFixture()
    {
        // Remove tmp file
        fs::remove("tmp.toml");
    }

protected:
    kb::cfg::Settings settings;
};

TEST_CASE_METHOD(SettingsFixture, "Loading a TOML file with default root name should set root name to filename stem",
                 "[cfg]")
{
    settings.load_toml("tmp.toml");

    REQUIRE(settings.get<std::string>("tmp.renderer.backend"_h, "None").compare("OpenGL") == 0);
}

TEST_CASE_METHOD(SettingsFixture, "Root name is overrideable", "[cfg]")
{
    settings.load_toml("tmp.toml", "erwin");

    REQUIRE(settings.get<std::string>("erwin.renderer.backend"_h, "None").compare("OpenGL") == 0);
}

TEST_CASE_METHOD(SettingsFixture, "Getting existing scalar properties should work", "[cfg]")
{
    settings.load_toml("tmp.toml");

    REQUIRE(settings.get<float>("tmp.my_float"_h, 0.f) == 1.42f);
    REQUIRE(settings.get<uint32_t>("tmp.my_uint"_h, 0) == 42);
    REQUIRE(settings.get<size_t>("tmp.my_uint"_h, 0) == 42);
    REQUIRE(settings.get<int32_t>("tmp.my_int"_h, 0) == -42);
    REQUIRE(settings.get<std::string>("tmp.my_string"_h, "").compare("Hello") == 0);
    REQUIRE(settings.is("tmp.my_bool"_h));
    REQUIRE_FALSE(settings.is("tmp.my_other_bool"_h));
}

TEST_CASE_METHOD(SettingsFixture, "Getting string hashes should work", "[cfg]")
{
    settings.load_toml("tmp.toml");

    REQUIRE(settings.get_hash("tmp.my_string"_h, "Nada") == "Hello"_h);
    REQUIRE(settings.get_hash_lower("tmp.my_string"_h, "nada") == "hello"_h);
    REQUIRE(settings.get_hash_upper("tmp.my_string"_h, "NADA") == "HELLO"_h);
}

TEST_CASE_METHOD(SettingsFixture, "Getting array properties should work", "[cfg]")
{
    settings.load_toml("tmp.toml");

    REQUIRE(settings.has_array("tmp.logger.channels"_h));
    size_t sz = settings.get_array_size("tmp.logger.channels"_h);
    REQUIRE(sz == 3);

    REQUIRE(settings.get<std::string>("tmp.logger.channels[0].name"_h, "").compare("application") == 0);
    REQUIRE(settings.get<std::string>("tmp.logger.channels[1].name"_h, "").compare("editor") == 0);
    REQUIRE(settings.get<std::string>("tmp.logger.channels[2].name"_h, "").compare("event") == 0);
    REQUIRE(settings.get<uint32_t>("tmp.logger.channels[0].verbosity"_h, 0) == 3);
    REQUIRE(settings.get<uint32_t>("tmp.logger.channels[1].verbosity"_h, 0) == 3);
    REQUIRE(settings.get<uint32_t>("tmp.logger.channels[2].verbosity"_h, 0) == 1);
}

TEST_CASE_METHOD(SettingsFixture, "Getting non-existing scalar properties should return default value", "[cfg]")
{
    settings.load_toml("tmp.toml");

    REQUIRE(settings.get<float>("tmp.my_non_existing_float"_h, 0.f) == 0.f);
    REQUIRE(settings.get<uint32_t>("tmp.my_non_existing_uint"_h, 0) == 0);
    REQUIRE(settings.get<size_t>("tmp.my_non_existing_uint"_h, 0) == 0);
    REQUIRE(settings.get<int32_t>("tmp.my_non_existing_int"_h, 0) == 0);
    REQUIRE(settings.get<std::string>("tmp.my_non_existing_string"_h, "").compare("") == 0);
    REQUIRE_FALSE(settings.is("tmp.my_non_existing_bool"_h));
}

TEST_CASE_METHOD(SettingsFixture, "Setting existing scalar properties should work", "[cfg]")
{
    settings.load_toml("tmp.toml");
    REQUIRE(settings.set<int32_t>("tmp.my_int"_h, -456));
    REQUIRE(settings.get<int32_t>("tmp.my_int"_h, 0) == -456);
    REQUIRE(settings.set<std::string>("tmp.my_string"_h, "Bye"));
    REQUIRE(settings.get<std::string>("tmp.my_string"_h, "").compare("Bye") == 0);
}

TEST_CASE_METHOD(SettingsFixture, "Setting array properties should work", "[cfg]")
{
    settings.load_toml("tmp.toml");
    REQUIRE(settings.set<uint32_t>("tmp.logger.channels[0].verbosity"_h, 0));
    REQUIRE(settings.get<uint32_t>("tmp.logger.channels[0].verbosity"_h, 42) == 0);
}

TEST_CASE_METHOD(SettingsFixture, "Saving scalar properties should work", "[cfg]")
{
    settings.load_toml("tmp.toml");
    settings.set<int32_t>("tmp.my_int"_h, -456);
    settings.set<std::string>("tmp.my_string"_h, "Bye");
    settings.set<bool>("tmp.my_other_bool"_h, true);
    settings.save_toml("tmp.toml");
    settings.clear();
    settings.load_toml("tmp.toml");

    REQUIRE(settings.get<int32_t>("tmp.my_int"_h, 0) == -456);
    REQUIRE(settings.get<std::string>("tmp.my_string"_h, "").compare("Bye") == 0);
    REQUIRE(settings.is("tmp.my_other_bool"_h));
}

TEST_CASE_METHOD(SettingsFixture, "Saving array properties should work", "[cfg]")
{
    settings.load_toml("tmp.toml");
    settings.set<uint32_t>("tmp.logger.channels[0].verbosity"_h, 0);
    settings.set<uint32_t>("tmp.logger.channels[1].verbosity"_h, 0);
    settings.set<uint32_t>("tmp.logger.channels[2].verbosity"_h, 0);
    settings.save_toml("tmp.toml");
    settings.clear();
    settings.load_toml("tmp.toml");

    REQUIRE(settings.get<uint32_t>("tmp.logger.channels[0].verbosity"_h, 42) == 0);
    REQUIRE(settings.get<uint32_t>("tmp.logger.channels[1].verbosity"_h, 42) == 0);
    REQUIRE(settings.get<uint32_t>("tmp.logger.channels[2].verbosity"_h, 42) == 0);
}