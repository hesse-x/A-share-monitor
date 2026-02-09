#include <sstream>
#include <string>
#include <string_view>

#include "logger.h"
#include "stock_fetcher.h"

#include <QNetworkRequest>
#include <QUrl>

class SinaFetcher : public NetworkFetcher {
public:
  SinaFetcher(std::string code)
      : NetworkFetcher(code, getRequest(getUrl(code))) {
    LOG(INFO) << "Creat fetcher: " << code;
  }
  ~SinaFetcher() = default;

private:
  StockInfo parseReturnInfo(std::string_view info) override final;

protected:
  // [name, curPrice, yesterdayPrice, openPrice]
  virtual int getNameIdx() const = 0;
  virtual int getCurPriceIdx() const = 0;
  virtual int getYesterdayPriceIdx() const = 0;
  virtual int getOpenPriceIdx() const = 0;

  static QNetworkRequest getRequest(QUrl url);
  static QUrl getUrl(std::string_view stockCode,
                     std::string_view prefix = std::string_view());
};
