#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "sina_fetcher.h"
#include "stock_fetcher.h"
#include "utils.h"

#include <QNetworkRequest>
#include <QString>
#include <QUrl>

QUrl SinaFetcher::getUrl(std::string_view stockCode, std::string_view prefix) {
  std::string urlHead = "http://hq.sinajs.cn/list=";
  if (!prefix.empty())
    urlHead.append(prefix);
  QUrl url(QString::fromStdString(urlHead.append(stockCode)));
  if (!url.isValid())
    throw std::invalid_argument(std::string("Invalid url: ").append(urlHead));

  return url;
}
// Construct API request URL with target stock code
QNetworkRequest SinaFetcher::getRequest(QUrl url) {
  // Configure request with browser-like headers to avoid 403 errors
  QNetworkRequest request(url);
  request.setRawHeader("Referer", "https://finance.sina.com.cn/");
  request.setRawHeader("User-Agent",
                       "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                       "AppleWebKit/537.36 (KHTML, like Gecko) "
                       "Chrome/120.0.0.0 Safari/537.36");
  return request;
}

static void getValue(const std::vector<std::string_view> inputs, int idx,
                     double &output) {
  if (idx < 0)
    idx = inputs.size() + idx;

  if (idx < 0 || idx >= static_cast<int>(inputs.size()))
    throw std::out_of_range("getValue idx out of range");

  auto sv = inputs[idx];
  const char *start = sv.data();
  const char *end = start + sv.size();
  char *cvtEnd;
  output = strtod(start, &cvtEnd);
  if (cvtEnd != end)
    throw std::out_of_range(std::string("invalid float format").append(sv));
}

static void getValue(const std::vector<std::string_view> inputs, int idx,
                     std::string &output) {
  if (idx < 0)
    idx = inputs.size() + idx;

  if (idx < 0 || idx >= static_cast<int>(inputs.size()))
    throw std::out_of_range("getValue idx out of range");

  output = gbk2utf8(inputs[idx]);
}

StockInfo SinaFetcher::parseReturnInfo(std::string_view response_data) {
  // Extract data between quotation marks from API response
  size_t start = response_data.find('"');
  size_t end = response_data.find('"', start + 1);
  if (start == std::string_view::npos || end == std::string_view::npos)
    throw std::invalid_argument("Invalid response data");
  auto stock_data = response_data.substr(start + 1, end - start - 1);
  auto fields = splitString(stock_data, ',');
  if (fields.size() < 4)
    throw std::length_error("Fetch result is too few");

  StockInfo result;
  getValue(fields, getCurPriceIdx(), result.curPrice);
  getValue(fields, getYesterdayPriceIdx(), result.yesterdayPrice);
  getValue(fields, getOpenPriceIdx(), result.openPrice);
  getValue(fields, getNameIdx(), result.name);
  return result;
}

class SinaStockFetcher final : public SinaFetcher {
  using SinaFetcher::SinaFetcher;
  ~SinaStockFetcher() = default;

  static bool regist;

private:
  int getNameIdx() const override final { return 0; }

  int getCurPriceIdx() const override final { return 3; }

  int getYesterdayPriceIdx() const override final { return 2; }

  int getOpenPriceIdx() const override final { return 1; }
};

// Register factory method for SinaFetcher
bool SinaStockFetcher::regist = StockFetcher::registCreator(
    StockFetcher::Type::kSina, [](std::string stockCode) -> StockFetcher * {
      return new SinaStockFetcher(stockCode);
    });
