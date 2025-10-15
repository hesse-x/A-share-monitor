#include <cstddef>
#include <set>
#include <utility>

#include "config_dialog.h"
#include "config_parser.h"
#include "stock.h"
#include "widget.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QInternal>
#include <QMenu>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QtGlobal>

#ifdef QT6_OR_NEWER
static inline std::pair<int, int> getDesktopSize() {
  QScreen *primaryScreen = QGuiApplication::primaryScreen();
  QRect rect = primaryScreen->geometry();
  int screenWidth = rect.width();
  int screenHeight = rect.height();
  return {screenWidth, screenHeight};
}
#else
#include <QDesktopWidget>
static inline std::pair<int, int> getDesktopSize() {
  QDesktopWidget *desktop = QApplication::desktop();
  int screenWidth = desktop->width();
  int screenHeight = desktop->height();
  return {screenWidth, screenHeight};
}
#endif

Widget::RollingDisplayState::RollingDisplayState() {}
Widget::RollingDisplayState::RollingDisplayState(
    const std::vector<std::string> &codes) {
  for (const auto &code : codes) {
    stocks.insert(std::make_unique<Stock>(code));
  }
  curIt = stocks.cbegin();
}

void Widget::RollingDisplayState::next() {
  if (++curIt == stocks.cend())
    curIt = stocks.cbegin();
}

Widget::~Widget() {}
Widget::Widget(const ConfigData &config, QWidget *parent)
    : QWidget(parent), m_dragging(false),
      dispalyType(DisplayMode::Type::kLineChart) {
  // Set window properties: borderless, no taskbar icon, transparent background,
  // always on top
  setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  for (size_t i = 0; i < displayMode.size(); i++) {
    displayMode[i] = std::unique_ptr<DisplayMode>(
        DisplayMode::create(static_cast<DisplayMode::Type>(i)));
  }

  // Create data manager
  state = RollingDisplayState(config.codes);
  connect(this, &Widget::dataUpdated, this, &Widget::onDataUpdated);
  connect(&updateTimer, &QTimer::timeout, this, &Widget::fetchLatestData);
  connect(&rollingTimer, &QTimer::timeout, this, &Widget::onDataUpdated);
  updateTimer.start(config.freq);

  // Create right-click menu items
  actions[static_cast<int>(MenuItemEnum::kShowLineChartPos)] =
      std::make_unique<QAction>("Show Line Chart", this);
  actions[static_cast<int>(MenuItemEnum::kShowDataOnlyPos)] =
      std::make_unique<QAction>("Show Data Only", this);
  actions[static_cast<int>(MenuItemEnum::kConfigPos)] =
      std::make_unique<QAction>("Config", this);
  actions[static_cast<int>(MenuItemEnum::kExitPos)] =
      std::make_unique<QAction>("Exit", this);

  connect(actions[static_cast<int>(MenuItemEnum::kShowLineChartPos)].get(),
          &QAction::triggered, this, &Widget::onShowLineChart);
  connect(actions[static_cast<int>(MenuItemEnum::kShowDataOnlyPos)].get(),
          &QAction::triggered, this, &Widget::onShowOnlyData);
  connect(actions[static_cast<int>(MenuItemEnum::kConfigPos)].get(),
          &QAction::triggered, this, &Widget::onConfig);
  connect(actions[static_cast<int>(MenuItemEnum::kExitPos)].get(),
          &QAction::triggered, this, &Widget::onExit);

  // Set window size
  updateWindowSize();
}

void Widget::updateWindowSize() {
  setScaledSize(); // Use original size calculation when showing line chart
}

void Widget::fetchLatestData() {
  for (auto &it : state.stocks) {
    it->fetchLatestData();
  }
  if (!needRolling()) {
    // Emit update.
    emit dataUpdated();
  }
}

void Widget::setScaledSize() {
  auto [screenWidth, screenHeight] = getDesktopSize();
  auto [finalWidth, finalHeight] =
      displayMode[static_cast<int>(dispalyType)]->calculateWindowSize(
          screenWidth, screenHeight, state.stocks.size());
  setFixedSize(finalWidth, finalHeight);
}

void Widget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  displayMode[static_cast<int>(dispalyType)]->paint(&painter, width(), height(),
                                                    state.stocks, state.curIt);
  if (needRolling())
    state.next();
}

void Widget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_dragging = true;
    m_dragStartPosition = event->pos();
  }
  QWidget::mousePressEvent(event);
}

void Widget::mouseMoveEvent(QMouseEvent *event) {
  if (m_dragging) {
    QPoint delta = event->pos() - m_dragStartPosition;
    move(mapToParent(delta));
  }
  QWidget::mouseMoveEvent(event);
}

void Widget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_dragging = false;
  }
  QWidget::mouseReleaseEvent(event);
}

void Widget::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);
  for (auto &action : actions) {
    menu.addAction(action.get());
  }
  menu.exec(event->globalPos());
}

void Widget::onShowLineChart() { switchTo<DisplayMode::Type::kLineChart>(); }

void Widget::onShowOnlyData() { switchTo<DisplayMode::Type::kDataOnly>(); }

void Widget::onDataUpdated() {
  // Refresh interface when data is updated
  update();
}

void Widget::onConfig() {
  ConfigDialog dialog(state.stocks, this);
  if (dialog.exec() == QDialog::Accepted) {
    auto deleted = dialog.getDeletedCodes();
    auto added = dialog.getAddedCodes();

    // Erase stock.
    for (const auto &code : deleted) {
      auto it = state.stocks.find(std::make_unique<Stock>(code));
      if (it != state.stocks.end()) {
        state.stocks.erase(it);
      }
    }

    // Insert stock.
    for (const auto &code : added) {
      state.stocks.insert(std::make_unique<Stock>(code));
    }

    // Update iter.
    state.curIt = state.stocks.begin();

    // Refresh UI.
    resetRolling();
    updateWindowSize();
    update();
    fetchLatestData();
    emit dataUpdated();
  }
}

void Widget::onExit() {
  // Exit application
  qApp->quit();
}

bool Widget::needRolling() const {
  return (dispalyType == DisplayMode::Type::kLineChart &&
          state.stocks.size() > 5) ||
         (dispalyType == DisplayMode::Type::kDataOnly &&
          state.stocks.size() > 1);
}

void Widget::resetRolling() {
  if (needRolling()) {
    rollingTimer.start(5000);
  } else {
    rollingTimer.stop();
  }
}
