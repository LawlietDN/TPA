#pragma once
#include <string>
#include <vector>

struct TrainSnapshot;
class StopManager;

class Dashboard
{
public:
    static std::string generate(std::vector<TrainSnapshot> const& stalledTrains,
                                StopManager& stops);

private:
    static int computeNowSec();
    static std::string buildHtmlHead(std::size_t stalledCount);
    static std::string buildTableHeader();
    static std::string formatLateness(TrainSnapshot const& t, int nowSec);
    static std::string buildRow(TrainSnapshot const& t, StopManager& stops, int nowSec);
};
