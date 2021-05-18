#include "RadiantTest.h"
#include "scene/PointTrace.h"

#include <sstream>

namespace test
{

using PointTraceTest = RadiantTest;

const std::string LIN_DATA = "544.000000 64.000000 112.000000\n"
                             "544.000000 64.000000 240.000000\n"
                             "512.000000 64.000000 240.000000\n"
                             "512.000000 64.000000 112.000000\n"
                             "544.000000 64.000000 112.000000\n";

TEST_F(PointTraceTest, ConstructPointTraceEmpty)
{
    std::string s("");
    std::istringstream ss(s);

    // Constructing with empty data should not crash, or add any undefined or
    // [0, 0, 0] points.
    map::PointTrace trace(ss);
    EXPECT_EQ(trace.size(), 0);
}

TEST_F(PointTraceTest, ConstructPointTraceWithData)
{
    // Construct a stream to read the data
    std::istringstream iss(LIN_DATA);

    // Construct the PointTrace to read the stream and confirm the expected
    // number of points are parsed
    map::PointTrace trace(iss);
    EXPECT_EQ(trace.size(), 5);
}

namespace
{

using StringList = std::list<std::string>;

// Get pointfile names in a list
StringList pointfiles()
{
    StringList result;
    GlobalMapModule().forEachPointfile([&](const std::string& pf)
                                       { result.push_back(pf); });
    return result;
}

}

TEST_F(PointTraceTest, IdentifyMapPointfiles)
{
    GlobalCommandSystem().executeCommand("OpenMap", std::string("altar.map"));

    // Check the number of pointfiles for this map
    EXPECT_EQ(pointfiles().size(), 2);
}

TEST_F(PointTraceTest, PointFilesAssociatedWithCorrectMap)
{
    std::string modRelativePath = "maps/altar_in_pk4.map";
    GlobalCommandSystem().executeCommand("OpenMap", modRelativePath);

    // No pointfiles should be associated with this map, even though it also
    // starts with "altar_"
    EXPECT_EQ(pointfiles().size(), 0);
}

}