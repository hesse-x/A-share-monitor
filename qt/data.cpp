#include <QDateTime>
#include <QRandomGenerator>

#include "data.h"
#include "stock_fetcher.h"

DataManager::DataManager(std::string stock_code, QObject *parent)
    : QObject(parent), baseData(0.0) {
  // Set up timer to update data every second
  connect(&updateTimer, &QTimer::timeout, this, &DataManager::fetchLatestData);
  updateTimer.start(60000);
  dataFetcher = new StockFetcher(stock_code);
  auto newData = dataFetcher->fetchData();
  if (!newData.empty()) {
    baseData = newData[1];
    datas.push_back(240, newData[2]);
  }
}

// Check if current time is within trading hours
bool DataManager::isTradingTime() const {
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

void DataManager::fetchLatestData() {
  // Do nothing during non-trading hours
  if (!isTradingTime()) {
    return;
  }

  auto newData = dataFetcher->fetchData();
  if (newData.empty()) { // Add empty check
    return;
  }

  datas.erase(datas.begin());
  datas.push_back(newData[2]);
  emit dataUpdated();
}

std::pair<double, double> DataManager::getBound() const {
  if (datas.empty()) { // Handle empty data case
    return {getBaseData(), getBaseData()};
  }

  double min = datas[0], max = datas[0];
  for (int i = 0; i < datas.size(); ++i) {
    min = std::min(min, datas[i]);
    max = std::max(max, datas[i]);
  }
  return {min, max};
}
