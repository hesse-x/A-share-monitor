#ifndef STOCK_FETCHER_H
#define STOCK_FETCHER_H

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include <QNetworkRequest>
#include <QUrl>

struct StockInfo {
  std::string name;
  double curPrice;
  double yesterdayPrice;
  double openPrice;
};

class StockFetcher {
public:
  enum class Type : int {
    kRandom = 0,
    kSina = 1,
    kSinaBackwardation = 2,
    kNum,
  };
  // Constructor: Initialize stock code
  explicit StockFetcher(std::string_view stockCode) : stockCode(stockCode) {}
  virtual ~StockFetcher() = default;

  // Fetch stock data once, return stock price
  virtual StockInfo fetchData() = 0;
  const std::string &getCode() const { return stockCode; }

  static StockFetcher *create(Type type, std::string stockCode);

protected:
  StockFetcher() {}

protected:
  std::string stockCode;
  static size_t writeCallback(void *contents, size_t size, size_t nmemb,
                              std::string *s);
  static bool registCreator(Type type,
                            std::function<StockFetcher *(std::string)> &&fn);
};

class NetworkFetcher : public StockFetcher {
public:
  StockInfo fetchData() override final;
  virtual ~NetworkFetcher() = default;
  void setUrl(QUrl url) { request.setUrl(url); }

protected:
  NetworkFetcher(std::string_view code, QNetworkRequest request)
      : StockFetcher(code), request(request) {}
  // Converts GBK encoded string to UTF-8
  virtual StockInfo parseReturnInfo(std::string_view info) = 0;

private:
  std::string fetch();
  QNetworkRequest request;
};

std::string gbk2utf8(std::string_view in);
#endif // STOCK_FETCHER_H
