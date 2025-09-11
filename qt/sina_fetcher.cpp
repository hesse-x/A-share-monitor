#include <array>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "logger.h"
#include "stock_fetcher.h"

#include <QNetworkRequest>
#include <QString>
#include <QUrl>

// Construct API request URL with target stock code
static QNetworkRequest getRequset(const std::string &stockCode) {
  QUrl url(QString::fromStdString("http://hq.sinajs.cn/list=" + stockCode));
  if (!url.isValid()) {
    LOG(ERROR) << "Invalid URL: " << stockCode;
  }

  // Configure request with browser-like headers to avoid 403 errors
  QNetworkRequest request(url);
  request.setRawHeader("Referer", "https://finance.sina.com.cn/");
  request.setRawHeader("User-Agent",
                       "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                       "AppleWebKit/537.36 (KHTML, like Gecko) "
                       "Chrome/120.0.0.0 Safari/537.36");
  return request;
}

class SinaStockFetcher final : public NetworkFetcher {
public:
  SinaStockFetcher(std::string code) : NetworkFetcher(code, getRequset(code)) {}
  ~SinaStockFetcher() = default;

  static bool regist;

private:
  std::optional<StockInfo> parseReturnInfo(std::string_view info,
                                           std::string *name) override;

  // Splits string into tokens using specified delimiter
  std::vector<std::string_view> split(std::string_view s, char delimiter);
};

std::vector<std::string_view> SinaStockFetcher::split(std::string_view s,
                                                      char delimiter) {
  std::vector<std::string_view> tokens;
  size_t start = 0;
  size_t end = s.find(delimiter);

  while (end != std::string_view::npos) {
    tokens.emplace_back(s.substr(start, end - start));
    start = end + 1;
    end = s.find(delimiter, start);
  }

  if (start < s.size()) {
    tokens.emplace_back(s.substr(start));
  }

  return tokens;
}

std::optional<StockInfo>
SinaStockFetcher::parseReturnInfo(std::string_view response_data,
                                  std::string *name) {
  std::optional<StockInfo> result = std::nullopt;

  // Extract data between quotation marks from API response
  size_t start = response_data.find('"');
  size_t end = response_data.find('"', start + 1);
  if (start != std::string_view::npos && end != std::string_view::npos) {
    auto stock_data = response_data.substr(start + 1, end - start - 1);
    auto fields = split(stock_data, ',');

    std::array<double, 8> infos;
    if (fields.size() >= 8) {
      // Parse numerical values from response fields
      for (int i = 1; i <= 7; ++i) {
        auto sv = fields[i];
        const char *start = sv.data();
        const char *end = start + sv.size();
        char *cvtEnd;
        infos[i] = strtod(start, &cvtEnd);
        if (cvtEnd != end)
          LOG(ERROR) << "invalid price format: " << sv;
      }

      LOG(INFO) << gbk2utf8(
          fields[0]); // Log stock name after encoding conversion
      if (name)
        *name = gbk2utf8(fields[0]);
      // Populate result with required stock information
      result = StockInfo{.curPrice = infos[3],
                         .yesterdayPrice = infos[2],
                         .openPrice = infos[1]};
    } else {
      LOG(ERROR) << "Data format error, insufficient fields, stock code: "
                 << stockCode;
    }
  } else {
    LOG(ERROR) << "Failed to parse response data";
  }
  return result;
}

// Register factory method for SinaStockFetcher
bool SinaStockFetcher::regist = StockFetcher::registCreator(
    StockFetcher::Type::kSina, [](std::string stockCode) -> StockFetcher * {
      return new SinaStockFetcher(stockCode);
    });
