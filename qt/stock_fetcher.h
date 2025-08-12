#ifndef STOCK_FETCHER_H
#define STOCK_FETCHER_H

#include <string>
#include <vector>

class StockFetcher {
private:
  std::string m_stockCode; // Stock code
  static size_t writeCallback(void *contents, size_t size, size_t nmemb,
                              std::string *s);
  std::vector<std::string> split(const std::string &s, char delimiter);

public:
  // Constructor: Pass stock code
  explicit StockFetcher(std::string stockCode);

  // Fetch stock data once, return fields 1-7 (float type)
  std::vector<float> fetchData();
};

#endif // STOCK_FETCHER_H
