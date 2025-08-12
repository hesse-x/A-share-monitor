#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <QObject>
#include <QTimer>
#include <vector>

#include "ring_buffer.h"

class StockFetcher;

using Data = ring_buffer<double, 240>;
class DataManager : public QObject {
  Q_OBJECT

public:
  explicit DataManager(std::string stock_code, QObject *parent = nullptr);

  double getCurrentNumber() const { return datas.back(); }
  double getBaseData() const { return baseData; }
  double getDifference() const { return getCurrentNumber() - getBaseData(); }
  double getPercentage() const {
    return getDifference() / getBaseData() * 100.0;
  }
  bool isBelow() const { return getDifference() < 0; }
  const Data &getData() const { return datas; }
  // Return {min, max}
  std::pair<double, double> getBound() const;

private slots:
  // Fetch new data and update
  void fetchLatestData();

signals:
  // Emitted when data is updated
  void dataUpdated();

private:
  double baseData; // Base value
  StockFetcher *dataFetcher;
  Data datas;         // Historical data
  QTimer updateTimer; // Timer for periodic updates

  bool isTradingTime() const;
  // Calculate difference
  void calculateDifference();
  // Calculate percentage change
  void calculatePercentage();
};

#endif // DATA_MANAGER_H
