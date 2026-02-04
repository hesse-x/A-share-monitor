#include <array>
#include <new>
#include <ostream>
#include <string>
#include <utility>

#include "logger.h"
#include "stock_fetcher.h"

#include <QByteArray>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QTextCodec>

// Callback function: Save response data
size_t StockFetcher::writeCallback(void *contents, size_t size, size_t nmemb,
                                   std::string *s) {
  size_t newLength = size * nmemb;
  try {
    s->append((char *)contents, newLength);
  } catch (const std::bad_alloc &e) {
    return 0;
  }
  return newLength;
}

static std::array<std::function<StockFetcher *(std::string)>,
                  static_cast<int>(StockFetcher::Type::kNum)>
    creators;

bool StockFetcher::registCreator(
    Type type, std::function<StockFetcher *(std::string)> &&fn) {
  creators[static_cast<int>(type)] = std::move(fn);
  return true;
}

std::string NetworkFetcher::fetch() {
  // Singleton QNetworkAccessManager for efficient network operations
  static QNetworkAccessManager manager;
  QNetworkReply *reply = manager.get(request);
  // Execute synchronous network request using event loop
  QEventLoop loop;
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();

  std::string response_data;
  // Process successful response
  if (reply->error() == QNetworkReply::NoError) {
    response_data = reply->readAll().toStdString();
  } else {
    LOG(ERROR) << "Request failed: " << reply->errorString().toStdString();
  }

  // Clean up network resources
  reply->deleteLater();
  return response_data;
}

std::optional<StockInfo> NetworkFetcher::fetchData(std::string *name) {
  auto result = fetch();
  return parseReturnInfo(result, name);
}

std::string NetworkFetcher::gbk2utf8(std::string_view in) {
  QByteArray gbkData(std::string(in).c_str(), in.size());
  QTextCodec *gbkCodec = QTextCodec::codecForName("GBK");
  if (!gbkCodec) {
    return "";
  }
  QString unicodeStr = gbkCodec->toUnicode(gbkData);
  return unicodeStr.toUtf8().toStdString();
}

StockFetcher *StockFetcher::create(Type type, std::string stockCode) {
  const auto &fn = creators[static_cast<int>(type)];
  if (!fn)
    LOG(FATAL) << "Invalid creator type";
  return fn(stockCode);
}
