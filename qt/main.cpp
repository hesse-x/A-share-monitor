#include <fstream>
#include <optional>

#include "config_parser.h"
#include "widget.h"

#include <QApplication>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  ConfigData config;

  if (argc == 2) {
    std::ifstream fin(argv[1]);
    auto configIn = parseConfig(fin);
    if (configIn.has_value())
      config = *configIn;
  }
  Widget widget(config);
  widget.show();

  return app.exec();
}
