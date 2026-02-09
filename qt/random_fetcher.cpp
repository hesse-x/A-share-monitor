#include <random>
#include <string>

#include "stock_fetcher.h"

constexpr double mu = 0.01;
constexpr double sigma = 0.02;
constexpr double init = 800.0;

double genRandom() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::normal_distribution<double> dist(mu, sigma);
  return dist(gen);
}

double genNextVal(double curVal) { return curVal * (1 + genRandom() / 100.0); }

class RandomStockFetcher : public StockFetcher {
public:
  RandomStockFetcher(std::string stockCode) : StockFetcher(stockCode) {
    yesterdayPrice = init;
    openPrice = genNextVal(yesterdayPrice);
    curPrice = openPrice;
  }
  ~RandomStockFetcher() = default;

  StockInfo fetchData() override;

  static bool regist;

private:
  double curPrice;
  double yesterdayPrice;
  double openPrice;
};

// Core method: Fetch data once and return fields 1-7
StockInfo RandomStockFetcher::fetchData() {
  curPrice = genNextVal(curPrice);
  return StockInfo{.name = "random",
                   .curPrice = curPrice,
                   .yesterdayPrice = yesterdayPrice,
                   .openPrice = openPrice};
}

bool RandomStockFetcher::regist = StockFetcher::registCreator(
    StockFetcher::Type::kRandom, [](std::string stockCode) -> StockFetcher * {
      return new RandomStockFetcher(stockCode);
    });
