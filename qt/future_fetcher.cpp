#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "logger.h"
#include "sina_fetcher.h"
#include "stock_fetcher.h"

#include <QDateTime>

static const std::map<std::string_view, std::string> kNameMap{
    {"IH", "sh000922"},
    {"IF", "sh000300"},
    {"IC", "sh000905"},
    {"IM", "sh000852"},
};

static std::pair<int, int> getNearestDate() {
  const QDate date = QDate::currentDate();
  int currentYear = date.year() % 100;
  int currentMonth = date.month();

  int remainder = currentMonth % 3;
  if (remainder != 0) {
    currentMonth = currentMonth + (3 - remainder);
  }
  return {currentYear, currentMonth};
}

class SinaFutureFetcher : public SinaFetcher {
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
  int getNameIdx() const override final { return -1; }

  int getCurPriceIdx() const override final { return 3; }

  int getYesterdayPriceIdx() const override final { return 0; }

  int getOpenPriceIdx() const override final { return 0; }

private:
  Type type;
  std::string futureCode;
};

static std::tuple<std::string, SinaFutureFetcher::Type>
parseCode(std::string_view code) {
  assert(code.size() > 3 && "Invalid future code");
  std::string_view name(code.data(), 2);
  std::string_view typeStr = code.substr(3);
  if (typeStr == "Front")
    return std::tuple<std::string, SinaFutureFetcher::Type>{
        name, SinaFutureFetcher::Type::kFront};
  if (typeStr == "Next")
    return std::tuple<std::string, SinaFutureFetcher::Type>{
        name, SinaFutureFetcher::Type::kNext};
  LOG(FATAL) << "Invalid future code";
  __builtin_unreachable();
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
  StockInfo fetchData() override final;
  virtual ~SinaBackwardationFetcher() = default;
  static bool regist;

private:
  std::unique_ptr<StockFetcher> spot;
  std::unique_ptr<SinaFutureFetcher> future;
};

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
  std::string ret(7, '\0');
  snprintf(ret.data(), ret.size(), "%2s%02d%02d", name.data(), year, month);
  ret.pop_back();
  return ret;
}

SinaFutureFetcher::SinaFutureFetcher(std::string_view name, Type fetchType)
    : SinaFetcher(getContractCode(name, fetchType)),
      futureCode(getContractCode(name, fetchType)){};

void SinaFutureFetcher::updateContract() {
  auto newCode = getContractCode(getCode(), type);
  setUrl(getUrl(newCode, "nf_"));
}

StockInfo SinaBackwardationFetcher::fetchData() {
  auto spotPrice = spot->fetchData();
  future->updateContract();
  auto futurePrice = future->fetchData();
  return StockInfo{.name = future->getContract(),
                   .curPrice = futurePrice.curPrice,
                   .yesterdayPrice = spotPrice.curPrice,
                   .openPrice = spotPrice.curPrice};
}

// Register factory method for SinaBackwardationFetcher
bool SinaBackwardationFetcher::regist = SinaBackwardationFetcher::registCreator(
    StockFetcher::Type::kSinaBackwardation,
    [](std::string stockCode) -> StockFetcher * {
      return new SinaBackwardationFetcher(stockCode);
    });
