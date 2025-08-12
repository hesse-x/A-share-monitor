#ifndef DISPLAY_STRATEGY_H
#define DISPLAY_STRATEGY_H

#include <cstdint>
#include <functional>
#include <utility>

#include "stock.h"

class QPainter;

class DisplayMode {
public:
  enum class Type : int {
    kLineChart = 0,
    kDataOnly = 1,
    kNum,
  };

  virtual ~DisplayMode() = default;

  // Return window size.
  virtual std::pair<int64_t, int64_t> calculateWindowSize(int64_t desktopWidth,
                                                          int64_t desktoHeight,
                                                          int64_t stockNum) = 0;

  // Paint window.
  virtual void paint(QPainter *painter, int64_t width, int64_t height,
                     const StockSet &stockSet, StockSetIt it) = 0;
  static DisplayMode *create(Type type);

protected:
  static bool registCreator(Type type, std::function<DisplayMode *()> &&fn);
};

#endif // DISPLAY_STRATEGY_H
