#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "logger.h"
#include "stock_fetcher.h"
#include "utils.h"

#include <QDateTime>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>

static const std::map<std::string_view, std::string> kNameMap{
    {"IH", "sh000922"},
    {"IF", "sh000300"},
    {"IC", "sh000905"},
    {"IM", "sh000852"},
};

class SinaFutureFetcher : public NetworkFetcher {
public:
  enum class Type : int {
    kFront = 0,
    kNext = 1,
  };

  SinaFutureFetcher(std::string_view name, Type fetchType);
  ~SinaFutureFetcher() = default;
  void updateContract();
  const std::string &getContract() const { return futureCode; }

private:
  std::optional<StockInfo> parseReturnInfo(std::string_view info,
                                           std::string *name) override;

private:
  Type type;
  std::string futureCode;
};

static std::tuple<std::string, SinaFutureFetcher::Type>
parseCode(std::string_view code) {
  std::string_view name(code.data(), 2);
  std::string_view typeStr = code.substr(3);
  if (typeStr == "Front")
    return std::tuple<std::string, SinaFutureFetcher::Type>{
        name, SinaFutureFetcher::Type::kFront};
  if (typeStr == "Next")
    return std::tuple<std::string, SinaFutureFetcher::Type>{
        name, SinaFutureFetcher::Type::kNext};
  return std::tuple<std::string, SinaFutureFetcher::Type>();
}

class SinaBackwardationFetcher : public StockFetcher {
public:
  SinaBackwardationFetcher(std::string stockCode) : StockFetcher(stockCode) {
    auto [code, type] = parseCode(stockCode);
    auto it = kNameMap.find(code);
    assert(it != kNameMap.end());
    spot = std::unique_ptr<StockFetcher>(
        StockFetcher::create(StockFetcher::Type::kSina, it->second));
    future = std::make_unique<SinaFutureFetcher>(it->first, type);
  }
  std::optional<StockInfo> fetchData(std::string *name) override final;
  virtual ~SinaBackwardationFetcher() = default;
  static bool regist;

private:
  std::unique_ptr<StockFetcher> spot;
  std::unique_ptr<SinaFutureFetcher> future;
};

static std::pair<int, int> getNearestDate() {
  const QDate date = QDate::currentDate();
  // 1. 提取指定日期的年份和月份
  int currentYear = date.year() % 100;
  int currentMonth = date.month();

  int remainder = currentMonth % 3; // 计算月份除以3的余数（0、1、2）
  if (remainder != 0) {
    currentMonth = currentMonth + (3 - remainder);
  }
  return {currentYear, currentMonth};
}

static std::string getContractCode(std::string_view name,
                                   SinaFutureFetcher::Type type) {
  auto date = getNearestDate();
  if (type == SinaFutureFetcher::Type::kNext) {
    date.second += 3;
  }
  auto [year, month] = date;
  if (month > 12) {
    year += 1;
    month = month % 12;
  }
  std::string ret(name);
  ret.append(4, '0');
  snprintf(ret.data() + 2, sizeof(ret.size() - 2), "%02d%02d", year, month);
  return ret;
}

static QUrl getUrl(const std::string &code) {
  return QUrl(QString::fromStdString("http://hq.sinajs.cn/list=nf_" + code));
}

static QNetworkRequest getRequest(const std::string &stockCode) {
  QUrl url = getUrl(stockCode);
  if (!url.isValid()) {
    LOG(ERROR) << "Invalid URL: " << stockCode;
  }

  QNetworkRequest request(url);
  request.setRawHeader("Referer", "https://finance.sina.com.cn/");
  request.setRawHeader("User-Agent",
                       "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                       "AppleWebKit/537.36 (KHTML, like Gecko) "
                       "Chrome/120.0.0.0 Safari/537.36");
  request.setRawHeader("Upgrade-Insecure-Requests", "1");
  return request;
}

SinaFutureFetcher::SinaFutureFetcher(std::string_view name, Type fetchType)
    : NetworkFetcher(name, getRequest(getContractCode(name, fetchType))),
      futureCode(getContractCode(name, fetchType)){};

void SinaFutureFetcher::updateContract() {
  auto newCode = getContractCode(getCode(), type);
  setUrl(getUrl(newCode));
}

std::optional<StockInfo>
SinaFutureFetcher::parseReturnInfo(std::string_view response_data,
                                   std::string *name) {
  std::optional<StockInfo> result = std::nullopt;

  // Extract data between quotation marks from API response
  size_t start = response_data.find('"');
  size_t end = response_data.find('"', start + 1);
  if (start != std::string_view::npos && end != std::string_view::npos) {
    auto stock_data = response_data.substr(start + 1, end - start - 1);
    auto fields = splitString(stock_data, ',');

    std::array<double, 8> infos;
    if (fields.size() >= 5) {
      // Parse numerical values from response fields
      for (int i = 0; i < 5; ++i) {
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
        *name = gbk2utf8(fields.back());
      // Populate result with required stock information
      result = StockInfo{.curPrice = infos[3],
                         .yesterdayPrice = infos[0],
                         .openPrice = infos[0]};
    } else {
      LOG(ERROR) << "Data format error, insufficient fields, stock code: "
                 << stockCode;
    }
  } else {
    LOG(ERROR) << "Failed to parse response data";
  }
  return result;
}

std::optional<StockInfo>
SinaBackwardationFetcher::fetchData(std::string *name) {
  auto spotPrice = spot->fetchData(nullptr);
  if (!spotPrice)
    return std::nullopt;
  future->updateContract();
  auto futurePrice = future->fetchData(nullptr);
  if (!futurePrice)
    return std::nullopt;
  if (name)
    *name = future->getContract();
  return StockInfo{.curPrice = futurePrice->curPrice,
                   .yesterdayPrice = spotPrice->curPrice,
                   .openPrice = spotPrice->curPrice};
}

// Register factory method for SinaBackwardationFetcher
bool SinaBackwardationFetcher::regist = SinaBackwardationFetcher::registCreator(
    StockFetcher::Type::kSinaBackwardation,
    [](std::string stockCode) -> StockFetcher * {
      return new SinaBackwardationFetcher(stockCode);
    });
