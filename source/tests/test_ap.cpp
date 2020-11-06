#include <catch2/catch_all.hpp>
#include "logger/logger.h"

using namespace kb;

TEST_CASE("Plop", "[plop]")
{
	KLOG("kibble",1) << "Hello" << std::endl;

	SUCCEED();
}