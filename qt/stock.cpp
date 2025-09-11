#include <algorithm>
#include <cstddef>
#include <optional>

#include "logger.h"
#include "stock.h"
#include "stock_fetcher.h"

#include <QDateTime>
#include <QInternal>

Stock::Stock(std::string stock_code) : baseData(0.0) {
  if (stock_code.starts_with("test")) {
    dataFetcher = std::unique_ptr<StockFetcher>(
        StockFetcher::create(StockFetcher::Type::kRandom, stock_code));
  } else {
    dataFetcher = std::unique_ptr<StockFetcher>(
        StockFetcher::create(StockFetcher::Type::kSina, stock_code));
  }
  auto newData = dataFetcher->fetchData(&name);
  if (!newData.has_value()) {
    LOG(ERROR) << "fetch data failed, stock_code: " << stock_code;
  }
  baseData = newData->yesterdayPrice;
  historyData.push_back(historyData.capacity(), newData->curPrice);
}

// Check if current time is within trading hours
bool Stock::isTradingTime() const {
  QDateTime now = QDateTime::currentDateTime();
  int day = now.date().dayOfWeek();

  // Only check from Monday to Friday
  if (day == Qt::Saturday || day == Qt::Sunday) {
    return false;
  }

  QTime time = now.time();
  // Morning trading session: 9:30-11:30
  bool morningSession = (time >= QTime(9, 30) && time <= QTime(11, 30));
  // Afternoon trading session: 13:00-15:00
  bool afternoonSession = (time >= QTime(13, 0) && time <= QTime(15, 0));

  return morningSession || afternoonSession;
}

void Stock::fetchLatestData() {
  // Do nothing during non-trading hours
  if (!isTradingTime()) {
    return;
  }
  auto newData = dataFetcher->fetchData();
  if (!newData.has_value()) { // Add empty check
    return;
  }
  if (newData->yesterdayPrice != baseData)
    baseData = newData->yesterdayPrice;
  historyData.push_back(newData->curPrice);
}

std::pair<double, double> Stock::getBound() const {
  if (historyData.empty()) { // Handle empty data case
    return {getBaseData(), getBaseData()};
  }

  double min = historyData[0], max = historyData[0];
  for (size_t i = 0; i < historyData.size(); ++i) {
    min = std::min(min, historyData[i]);
    max = std::max(max, historyData[i]);
  }
  return {min, max};
}
