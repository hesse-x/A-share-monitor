#ifndef CONFIG_DIALOG_H
#define CONFIG_DIALOG_H

#include <QDialog>
#include <set>
#include <string>
#include <vector>

#include "stock.h"

#include <QMetaObject>
#include <QString>
#include <QtGlobal>
class QObject;
class QWidget;

QT_BEGIN_NAMESPACE
namespace Ui {
class ConfigDialog;
}
QT_END_NAMESPACE

class ConfigDialog : public QDialog {
  Q_OBJECT
public:
  ConfigDialog(const StockSet &currentStocks, QWidget *parent = nullptr);
  virtual ~ConfigDialog() = default;

  std::set<std::string> getDeletedCodes() const { return deletedCodes; }
  std::vector<std::string> getAddedCodes() const { return addedCodes; }

private slots:
  void on_addButton_clicked();
  void on_removeButton_clicked();
  void on_resetButton_clicked();

private:
  Ui::ConfigDialog *ui;
  const StockSet &originalStocks;      // Original Stocks(read only)
  std::set<std::string> originalCodes; // Original stock code.
  std::set<std::string> deletedCodes;  // Record deleted stock code.
  std::vector<std::string> addedCodes; // Record added stock code.

  void refreshList();
};
#endif // CONFIG_DIALOG_H
