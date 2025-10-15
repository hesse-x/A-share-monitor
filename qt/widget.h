#ifndef WIDGET_H
#define WIDGET_H
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "display_mode.h"
#include "stock.h"

#include <QMetaObject>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QWidget>

class QAction;
class ConfigData;
class QContextMenuEvent;
class QMouseEvent;
class QObject;
class QPaintEvent;

class Widget : public QWidget {
  Q_OBJECT

public:
  explicit Widget(const ConfigData &config, QWidget *parent = nullptr);
  ~Widget() override;

private:
  void setScaledSize();
  void updateWindowSize(); // Update window size
  void fetchLatestData();
  bool needRolling() const;
  void resetRolling();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void
  contextMenuEvent(QContextMenuEvent *event) override; // Right-click menu event
  template <DisplayMode::Type type> void switchTo() {
    dispalyType = type;
    resetRolling();
    updateWindowSize();
    update();
  }

private slots:
  void onDataUpdated();
  void onShowLineChart(); // Show line chart
  void onShowOnlyData();  // Show data only
  void onConfig();        // Config
  void onExit();          // Exit

signals:
  // Emitted when data is updated
  void dataUpdated();

private:
  struct RollingDisplayState {
    RollingDisplayState();
    explicit RollingDisplayState(const std::vector<std::string> &codes);
    void next();
    StockSet stocks;
    StockSetIt curIt;
  };

  bool m_dragging;
  DisplayMode::Type dispalyType; // Flag for showing line chart
  RollingDisplayState state;
  QTimer updateTimer;  // Timer for periodic updates
  QTimer rollingTimer; // Timer for periodic updates
  QPoint m_dragStartPosition;
  // Menu item {"Show line chart", "Show data only", "Config", "Exit"}
  enum class MenuItemEnum : int {
    kShowLineChartPos = 0,
    kShowDataOnlyPos,
    kConfigPos,
    kExitPos,
    kNum
  };
  std::array<std::unique_ptr<QAction>, static_cast<int>(MenuItemEnum::kNum)>
      actions;
  std::array<std::unique_ptr<DisplayMode>,
             static_cast<int>(DisplayMode::Type::kNum)>
      displayMode;
};

#endif // WIDGET_H
