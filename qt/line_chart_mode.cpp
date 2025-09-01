#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "display_mode.h"
#include "ring_buffer.h"
#include "stock.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QInternal>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QString>
#include <QtGlobal>

static const QColor transparentColor = QColor(255, 0, 0, 0);
constexpr int pad = 10;
class LineChartMode final : public DisplayMode {
public:
  std::pair<int64_t, int64_t> calculateWindowSize(int64_t desktopWidth,
                                                  int64_t desktoHeight,
                                                  int64_t stockNum) override;
  void paint(QPainter *painter, int64_t width, int64_t height,
             const StockSet &stockSet, StockSetIt it) override;

  static bool regist;

private:
  void drawSingleLineChart(QPainter *painter, const QColor &color,
                           const Stock *stock, int startX, int startY,
                           int width, int height);
  void drawSingleTextNumbers(QPainter *painter, const QColor &color,
                             const Stock *stock, int startY, int width,
                             int height);
  static inline int64_t calDisplayNum(int64_t totalNum) {
    return std::min(totalNum, int64_t(5));
  }
};

std::pair<int64_t, int64_t>
LineChartMode::calculateWindowSize(int64_t desktopWidth, int64_t desktoHeight,
                                   int64_t stockNum) {
  int targetWidthByScreen = desktopWidth / 6;
  int targetHeightByScreen = desktoHeight / 10;

  constexpr float ratio = 3.2;
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
  finalHeight = (qMax(finalHeight, 80) + pad) * calDisplayNum(stockNum);

  return {finalWidth, finalHeight};
}

void LineChartMode::paint(QPainter *painter, int64_t width, int64_t height,
                          const StockSet &stockSet, StockSetIt it) {
  constexpr int alpha = 255 * 0.6;
  static const QColor redColor = QColor(255, 0, 0, alpha);
  static const QColor greenColor = QColor(0, 255, 0, alpha);

  int64_t displayNum = calDisplayNum(stockSet.size());
  // Layout settings: text on left (20% width), line chart on right (80%
  // width)
  int64_t numberAreaWidth = width / 5;
  int64_t graphWidth = width - numberAreaWidth;
  int64_t graphHeight = height / displayNum;
  int64_t graphStartX = numberAreaWidth;
  int64_t graphStartY = 0;

  for (int i = 0; i < displayNum; i++) {
    const Stock *stock = it->get();
    QColor color = redColor;
    if (stock->isBelow())
      color = greenColor;
    // Draw line chart
    drawSingleLineChart(painter, color, stock, graphStartX, graphStartY,
                        graphWidth, graphHeight - pad);
    // Draw text
    drawSingleTextNumbers(painter, color, stock, graphStartY, numberAreaWidth,
                          graphHeight - pad);
    graphStartY += graphHeight;
    ++it;
    if (it == stockSet.cend())
      it = stockSet.cbegin();
  }
}

void LineChartMode::drawSingleLineChart(QPainter *painter, const QColor &color,
                                        const Stock *stock, int startX,
                                        int startY, int width, int height) {
  const auto &numbers = stock->getHistroy();
  if (numbers.empty())
    return;

  auto [min, max] = stock->getBound();
  double baseData = stock->getBaseData();
  double ub = std::max(max, baseData);
  double lb = std::min(min, baseData);
  if (ub == lb) {
    ub += baseData * 0.01;
    lb -= baseData * 0.01;
  }
  double diff = ub - lb;

  double xStep = static_cast<double>(width) / (numbers.size() - 1);

  // Create red gradient
  QLinearGradient gradient(startX, startY, startX, startY + height / 2);
  gradient.setColorAt(0, color);            // Red, alpha=0.8
  gradient.setColorAt(1, transparentColor); // Fully transparent

  // Draw gradient filled area
  QBrush brush(gradient);
  painter->setBrush(brush);
  painter->setPen(Qt::NoPen);

  // Draw region.
  QPainterPath fillPath;
  fillPath.moveTo(startX, startY + height);
  for (size_t i = 0; i < numbers.size(); ++i) {
    double x = startX + i * xStep;
    double y = startY + ((ub - numbers[i]) / diff) * height;
    fillPath.lineTo(x, y);
  }

  fillPath.lineTo(startX + width, startY + height);
  fillPath.closeSubpath();
  painter->drawPath(fillPath);

  // Draw line
  QPen pen(color, 1);
  painter->setPen(pen);
  painter->setBrush(Qt::NoBrush);

  QPainterPath linePath;
  for (size_t i = 0; i < numbers.size(); ++i) {
    double x = startX + i * xStep;
    double y = startY + ((ub - numbers[i]) / diff) * height;
    if (i == 0) {
      linePath.moveTo(x, y);
    } else {
      linePath.lineTo(x, y);
    }
  }
  painter->drawPath(linePath);
}

void LineChartMode::drawSingleTextNumbers(QPainter *painter,
                                          const QColor &color,
                                          const Stock *stock, int startY,
                                          int width, int height) {
  //  startY += height / 10;
  // Determine font size
  int baseFontSize = qMin(height / 5, width / 6);
  baseFontSize = qMax(baseFontSize, 9);

  // Calculate text position and line spacing
  int lineSpacing =
      static_cast<int>(baseFontSize * 0.3); // Compact line spacing

  // Set font
  painter->setPen(QPen(color));
  painter->setBrush(Qt::NoBrush);
  painter->setFont(QFont("Arial Narrow", baseFontSize, QFont::Bold));

  int curY = startY;
  // First line: stock name
  const auto &name = stock->getName();
  QString nameText(QString::fromUtf8(name.data(), name.size()));
  if (nameText.length() >= 4)
    nameText = nameText.left(2) + "...";
  painter->drawText(0, curY, width, baseFontSize, Qt::AlignCenter, nameText);
  curY += (baseFontSize + lineSpacing);

  // First line: current value
  QString currentText = QString("%1").arg(stock->getCurrentNumber(), 0, 'f', 2);
  painter->drawText(0, curY, width, baseFontSize, Qt::AlignCenter, currentText);
  curY += (baseFontSize + lineSpacing);

  // Second line: difference
  auto diff = stock->getDifference();
  QString diffText = QString("%1").arg(diff, 0, 'f', 2);
  if (diff >= 0) {
    diffText = "+" + diffText;
  }
  painter->drawText(0, curY, width, baseFontSize, Qt::AlignCenter, diffText);
  curY += (baseFontSize + lineSpacing);

  // Third line: percentage
  QString percentText =
      QString("%1%").arg(stock->getPercentage(), 0, 'f', 2, '+');
  if (diff >= 0) {
    percentText = "+" + percentText;
  }
  painter->drawText(0, curY, width, baseFontSize, Qt::AlignCenter, percentText);
}

bool LineChartMode::regist = DisplayMode::registCreator(
    DisplayMode::Type::kLineChart,
    []() -> DisplayMode * { return new LineChartMode; });
