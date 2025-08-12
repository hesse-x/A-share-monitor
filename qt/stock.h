#ifndef STOCK_H
#define STOCK_H

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "ring_buffer.h"
#include "stock_fetcher.h"

using Data = ring_buffer<double, 240>;

class Stock {
public:
  explicit Stock(std::string stock_code);

  double getCurrentNumber() const { return historyData.back(); }
  double getBaseData() const { return baseData; }
  double getDifference() const { return getCurrentNumber() - getBaseData(); }
  double getPercentage() const {
    return getDifference() / getBaseData() * 100.0;
  }
  bool isBelow() const { return getDifference() < 0; }
  const Data &getHistroy() const { return historyData; }
  const std::string &getCode() const { return dataFetcher->getCode(); }
  const std::string &getName() const { return name; }
  bool operator<(const Stock &other) const {
    return getCode() < other.getCode();
  }
  bool operator>(const Stock &other) const {
    return getCode() > other.getCode();
  }
  bool operator==(const Stock &other) const {
    return getCode() == other.getCode();
  }
  // Return {min, max}
  std::pair<double, double> getBound() const;
  ~Stock() = default;

  // Fetch new data and update
  void fetchLatestData();

private:
  double baseData; // Base value
  std::unique_ptr<StockFetcher> dataFetcher;
  Data historyData; // Historical data
  std::string name;

  bool isTradingTime() const;
  // Calculate difference
  void calculateDifference();
  // Calculate percentage change
  void calculatePercentage();
};

struct StockCmp {
  inline bool operator()(const std::unique_ptr<Stock> &a,
                         const std::unique_ptr<Stock> &b) const {
    return *a < *b;
  }
};
using StockSet = std::set<std::unique_ptr<Stock>, StockCmp>;
using StockSetIt = StockSet::const_iterator;

#endif // STOCK_H
