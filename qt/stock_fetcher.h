#ifndef STOCK_FETCHER_H
#define STOCK_FETCHER_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <QNetworkRequest>

struct StockInfo {
  double curPrice;
  double yesterdayPrice;
  double openPrice;
};

class StockFetcher {
public:
  enum class Type : int {
    kRandom = 0,
    kSina = 1,
    kNum,
  };
  // Constructor: Initialize stock code
  explicit StockFetcher(std::string stockCode) : stockCode(stockCode) {}
  virtual ~StockFetcher() = default;

  // Fetch stock data once, return stock price
  virtual std::optional<StockInfo> fetchData(std::string *name = nullptr) = 0;
  const std::string &getCode() const { return stockCode; }

  static StockFetcher *create(Type type, std::string stockCode);

protected:
  std::string stockCode;
  static size_t writeCallback(void *contents, size_t size, size_t nmemb,
                              std::string *s);
  static bool registCreator(Type type,
                            std::function<StockFetcher *(std::string)> &&fn);
};

class NetworkFetcher : public StockFetcher {
public:
  std::optional<StockInfo> fetchData(std::string *name) override final;
  virtual ~NetworkFetcher() = default;

protected:
  NetworkFetcher(std::string code, QNetworkRequest request)
      : StockFetcher(code), request(request) {}
  // Converts GBK encoded string to UTF-8
  std::string gbk2utf8(std::string_view in);
  virtual std::optional<StockInfo> parseReturnInfo(std::string_view info,
                                                   std::string *name) = 0;

private:
  std::string fetch();
  QNetworkRequest request;
};
#endif // STOCK_FETCHER_H
