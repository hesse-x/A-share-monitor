#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "display_mode.h"
#include "stock.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QInternal>
#include <QPainter>
#include <QPen>
#include <QString>
#include <QtGlobal>

class DataOnlyMode final : public DisplayMode {
public:
  std::pair<int64_t, int64_t> calculateWindowSize(int64_t desktopWidth,
                                                  int64_t desktoHeight,
                                                  int64_t stockNum) override;
  void paint(QPainter *painter, int64_t width, int64_t height,
             const StockSet &stockSet, StockSetIt it) override;

  static bool regist;

private:
  void drawSingleTextNumbers(QPainter *painter, const QColor &color,
                             const Stock *stock, int startY, int width,
                             int height);
};

constexpr int lineNum = 4;

std::pair<int64_t, int64_t>
DataOnlyMode::calculateWindowSize(int64_t desktopWidth, int64_t desktoHeight,
                                  int64_t stockNum) {
  // Calculate minimum size when showing only data
  int baseFontSize = 12; // Base font size
  int lineSpacing = static_cast<int>(baseFontSize * 0.3);
  int totalTextHeight = baseFontSize * (lineNum + 1) + lineSpacing * lineNum;

  // Calculate required text width (assuming the longest text is percentage
  // with 1 decimal place)
  QString sampleText = "+123.45%"; // Sample text for width calculation
  QFont font("Arial Narrow", baseFontSize, QFont::Bold);
  QFontMetrics fm(font);
#ifdef QT6_OR_NEWER
  int textWidth = fm.horizontalAdvance(sampleText) + 10; // Qt6 recommended
#else
  int textWidth = fm.width(sampleText) + 10; // Qt5 compatible
#endif
  return {textWidth, totalTextHeight};
}

void DataOnlyMode::paint(QPainter *painter, int64_t width, int64_t height,
                         const StockSet &stockSet, StockSetIt it) {
  QColor color;
  constexpr int alpha = 255 * 0.6;
  static const QColor redColor = QColor(255, 0, 0, alpha);
  static const QColor greenColor = QColor(0, 255, 0, alpha);

  const Stock *stock = it->get();
  color = redColor;
  if (stock->isBelow())
    color = greenColor;

  drawSingleTextNumbers(painter, color, stock, 0, width, height);
}

void DataOnlyMode::drawSingleTextNumbers(QPainter *painter, const QColor &color,
                                         const Stock *stock, int startY,
                                         int width, int height) {
  // Determine font size
  int baseFontSize = qMin(height / 5, width / 6);
  baseFontSize = qMax(baseFontSize, 9);

  // Calculate text position and line spacing
  int totalTextHeight = baseFontSize * lineNum;
  startY += (height - totalTextHeight) / 2;
  int lineSpacing =
      static_cast<int>(baseFontSize * 0.3); // Compact line spacing

  // Set font
  painter->setPen(QPen(color));
  painter->setBrush(Qt::NoBrush);
  painter->setFont(QFont("Arial Narrow", baseFontSize, QFont::Bold));

  int lineHeight = baseFontSize + lineSpacing;
  const auto &name = stock->getName();
  QString nameText(QString::fromUtf8(name.data(), name.size()));
  if (nameText.length() >= 4)
    nameText = nameText.left(2) + "...";
  painter->drawText(0, startY, width, baseFontSize, Qt::AlignCenter, nameText);
  startY += lineHeight;

  // First line: current value
  int64_t n = 2;
  auto curPrice = stock->getCurrentNumber();
  if (curPrice < 10)
    n = 3;
  QString currentText = QString("%1").arg(curPrice, 0, 'f', n);
  painter->drawText(0, startY, width, baseFontSize, Qt::AlignCenter,
                    currentText);
  startY += lineHeight;

  // Second line: difference
  auto diff = stock->getDifference();
  QString diffText = QString("%1").arg(diff, 0, 'f', n);
  if (diff >= 0) {
    diffText = "+" + diffText;
  }
  painter->drawText(0, startY, width, baseFontSize, Qt::AlignCenter, diffText);
  startY += lineHeight;

  // Third line: percentage
  QString percentText =
      QString("%1%").arg(stock->getPercentage(), 0, 'f', 2, '+');
  if (diff >= 0) {
    percentText = "+" + percentText;
  }
  painter->drawText(0, startY, width, baseFontSize, Qt::AlignCenter,
                    percentText);
}

bool DataOnlyMode::regist = DisplayMode::registCreator(
    DisplayMode::Type::kDataOnly,
    []() -> DisplayMode * { return new DataOnlyMode; });
