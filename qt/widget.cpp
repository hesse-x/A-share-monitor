#include "widget.h"
#include "macro.h"
#include <QApplication>
#include <QBrush>
#include <QContextMenuEvent>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <iostream>
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

static const QColor transparentColor = QColor(255, 0, 0, 0);
Widget::Widget(QWidget *parent)
    : QWidget(parent), m_dragging(false), m_showLineChart(true) {
  // Set window properties: borderless, no taskbar icon, transparent background,
  // always on top
  setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);

  // Create data manager
  dataManager = new DataManager("sh688256", this);
  connect(dataManager, &DataManager::dataUpdated, this, &Widget::onDataUpdated);

  // Create right-click menu items
  m_actionShowLine = new QAction("Show Line Chart", this);
  m_actionShowData = new QAction("Show Data Only", this);
  m_actionExit = new QAction("Exit", this);

  connect(m_actionShowLine, &QAction::triggered, this,
          &Widget::onShowLineChart);
  connect(m_actionShowData, &QAction::triggered, this, &Widget::onShowOnlyData);
  connect(m_actionExit, &QAction::triggered, this, &Widget::onExit);

  // Set window size
  updateWindowSize();
}

Widget::~Widget() {
  // Data manager will be automatically destroyed by parent object
}

void Widget::updateWindowSize() {
  if (m_showLineChart) {
    setScaledSize(); // Use original size calculation when showing line chart
  } else {
    // Calculate minimum size when showing only data
    int baseFontSize = 12; // Base font size
    int lineSpacing = static_cast<int>(baseFontSize * 0.3);
    int totalTextHeight = baseFontSize * 3 + lineSpacing * 2;

    // Calculate required text width (assuming the longest text is percentage
    // with 1 decimal place)
    QString sampleText = "+123.45%"; // Sample text for width calculation
    QFont font("Arial Narrow", baseFontSize, QFont::Bold);
    QFontMetrics fm(font);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    int textWidth = fm.horizontalAdvance(sampleText) + 10; // Qt6 recommended
#else
    int textWidth = fm.width(sampleText) + 10; // Qt5 compatible
#endif

    setFixedSize(textWidth, totalTextHeight);
  }
}

void Widget::setScaledSize() {
  auto [screenWidth, screenHeight] = getDesktopSize();
  int targetHeightByScreen = screenHeight / 10;
  int targetWidthByScreen = screenWidth / 18;

  constexpr float ratio = 3;
  int widthByRatio = targetHeightByScreen * ratio;
  int heightByRatio = targetWidthByScreen / ratio;

  int finalWidth, finalHeight;

  if (widthByRatio <= targetWidthByScreen) {
    finalWidth = widthByRatio;
    finalHeight = targetHeightByScreen;
  } else {
    finalWidth = targetWidthByScreen;
    finalHeight = heightByRatio;
  }

  // Minimum size
  finalWidth = qMax(finalWidth, 250);
  finalHeight = qMax(finalHeight, 80);

  setFixedSize(finalWidth, finalHeight);
}

void Widget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  QColor color;
  constexpr int alpha = 255 * 0.6;
  static const QColor redColor = QColor(255, 0, 0, alpha);
  static const QColor greenColor = QColor(0, 255, 0, alpha);

  color = redColor;
  if (dataManager->isBelow())
    color = greenColor;

  if (m_showLineChart) {
    // Layout settings: text on left (20% width), line chart on right (80%
    // width)
    int numberAreaWidth = width() / 5;
    int graphWidth = width() - numberAreaWidth;
    int graphHeight = height();
    int graphStartX = numberAreaWidth;

    // Draw line chart
    drawLineChart(&painter, color, graphStartX, graphWidth, graphHeight);

    // Draw text
    drawTextNumbers(&painter, color, numberAreaWidth, height());
  } else {
    // Show only data
    drawTextNumbers(&painter, color, width(), height());
  }
}

void Widget::drawLineChart(QPainter *painter, const QColor &color, int startX,
                           int width, int height) {
  // Create red gradient
  QLinearGradient gradient(startX, 0, startX, height);
  gradient.setColorAt(0, color);            // Red, alpha=0.8
  gradient.setColorAt(1, transparentColor); // Fully transparent

  // Draw gradient filled area
  QBrush brush(gradient);
  painter->setBrush(brush);
  painter->setPen(Qt::NoPen);

  QPainterPath fillPath;
  fillPath.moveTo(startX, height);

  const auto &numbers = dataManager->getData();
  if (numbers.empty())
    return;

  double xStep = static_cast<double>(width) / (numbers.size() - 1);

  auto [min, max] = dataManager->getBound();
  double baseData = dataManager->getBaseData();
  double ub = std::max(max, baseData);
  double lb = std::min(min, baseData);
  if (ub == lb) {
    ub += baseData * 0.01;
    lb -= baseData * 0.01;
  }
  double diff = ub - lb;
  fillPath.moveTo(startX, height);
  for (int i = 0; i < numbers.size(); ++i) {
    double x = startX + i * xStep;
    double y = height - ((numbers[i] - lb) / diff) * height;
    fillPath.lineTo(x, y);
  }

  fillPath.lineTo(startX + width, height);
  fillPath.closeSubpath();
  painter->drawPath(fillPath);

  // Draw line
  QPen pen(color, 1);
  painter->setPen(pen);
  painter->setBrush(Qt::NoBrush);

  QPainterPath linePath;
  for (int i = 0; i < numbers.size(); ++i) {
    double x = startX + i * xStep;
    double y = height - ((numbers[i] - lb) / diff) * height;
    if (i == 0) {
      linePath.moveTo(x, y);
    } else {
      linePath.lineTo(x, y);
    }
  }
  painter->drawPath(linePath);
}

void Widget::drawTextNumbers(QPainter *painter, const QColor &color,
                             int areaWidth, int areaHeight) {
  // Determine font size
  int baseFontSize = qMin(areaHeight / 5, areaWidth / 6);
  baseFontSize = qMax(baseFontSize, 9);

  // Calculate text position and line spacing
  int totalTextHeight = baseFontSize * 3;
  int startY = (areaHeight - totalTextHeight) / 2;
  int lineSpacing =
      static_cast<int>(baseFontSize * 0.3); // Compact line spacing

  // Set font
  painter->setPen(QPen(color));
  painter->setBrush(Qt::NoBrush);
  painter->setFont(QFont("Arial Narrow", baseFontSize, QFont::Bold));

  // First line: current value
  QString currentText =
      QString("%1").arg(dataManager->getCurrentNumber(), 0, 'f', 2);
  painter->drawText(0, startY, areaWidth, baseFontSize, Qt::AlignCenter,
                    currentText);

  // Second line: difference
  auto diff = dataManager->getDifference();
  QString diffText = QString("%1").arg(diff, 0, 'f', 2);
  if (diff >= 0) {
    diffText = "+" + diffText;
  }
  painter->drawText(0, startY + baseFontSize + lineSpacing, areaWidth,
                    baseFontSize, Qt::AlignCenter, diffText);

  // Third line: percentage
  QString percentText =
      QString("%1%").arg(dataManager->getPercentage(), 0, 'f', 2, '+');
  if (diff >= 0) {
    percentText = "+" + percentText;
  }
  painter->drawText(0, startY + baseFontSize * 2 + lineSpacing * 2, areaWidth,
                    baseFontSize, Qt::AlignCenter, percentText);
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
  menu.addAction(m_actionShowLine);
  menu.addAction(m_actionShowData);
  menu.addAction(m_actionExit);
  menu.exec(event->globalPos());
}

void Widget::onShowLineChart() {
  m_showLineChart = true;
  updateWindowSize();
  update();
}

void Widget::onShowOnlyData() {
  m_showLineChart = false;
  updateWindowSize();
  update();
}

void Widget::onDataUpdated() {
  // Refresh interface when data is updated
  update();
}

void Widget::onExit() {
  // Exit application
  qApp->quit();
}
