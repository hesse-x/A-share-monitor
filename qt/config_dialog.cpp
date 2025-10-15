#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>

#include "config_dialog.h"
#include "ui_config_dialog.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QMessageBox>

class QWidget;
ConfigDialog::ConfigDialog(const StockSet &currentStocks, QWidget *parent)
    : QDialog(parent), ui(new Ui::ConfigDialog), originalStocks(currentStocks) {
  ui->setupUi(this);
  setWindowTitle("Configure Stock List");

  // Extract original stock codes (for quick validation)
  for (const auto &stock : originalStocks) {
    originalCodes.insert(stock->getCode());
  }

  // Initially display all original stocks
  refreshList();
}

// Refresh list: original codes - deleted + added
void ConfigDialog::refreshList() {
  ui->listWidget->clear();
  // Display original stocks (not marked for deletion)
  for (const auto &stock : originalStocks) {
    std::string code = stock->getCode();
    if (!deletedCodes.count(code)) {
      // Get name directly from Stock object, show "--" if empty
      std::string name = stock->getName().empty() ? "--" : stock->getName();
      ui->listWidget->addItem(QString::fromStdString(code + " " + name));
    }
  }
  // Display added stocks (fixed name as "--")
  for (const auto &code : addedCodes) {
    ui->listWidget->addItem(QString::fromStdString(code + " -- (new)"));
  }
}

// Add stock (record to addedCodes, check for duplicates)
void ConfigDialog::on_addButton_clicked() {
  bool ok;
  QString code = QInputDialog::getText(
      this, "Add Stock",
      "Please enter stock code (format: sh600000 or sz000000):",
      QLineEdit::Normal, "", &ok);
  if (ok && !code.isEmpty()) {
    std::string codeStr = code.trimmed().toStdString();
    bool valid = true;
    QString errorMsg;

    // Validation 1: Length must be 8 characters
    if (codeStr.size() != 8) {
      valid = false;
      errorMsg = "Code length must be 8 characters (e.g., sh600000)";
    }
    // Validation 2: First two characters must be 'sh' (Shanghai) or 'sz'
    // (Shenzhen)
    else if (codeStr.substr(0, 2) != "sh" && codeStr.substr(0, 2) != "sz") {
      valid = false;
      errorMsg =
          "First two characters must be 'sh' (Shanghai) or 'sz' (Shenzhen)";
    }
    // Validation 3: Last six characters must be digits
    else {
      std::string numPart = codeStr.substr(2, 6);
      for (char c : numPart) {
        if (!isdigit(c)) { // Check if it's a digit
          valid = false;
          errorMsg = "Last six characters must be digits (0-9)";
          break;
        }
      }
    }

    // Show error message if validation fails
    if (!valid) {
      QMessageBox::warning(this, "Format Error", errorMsg);
      return;
    }

    // Duplication check (existing stocks or already added)
    bool isDuplicate = originalCodes.count(codeStr);
    if (!isDuplicate) {
      isDuplicate = std::find(addedCodes.begin(), addedCodes.end(), codeStr) !=
                    addedCodes.end();
    }
    if (isDuplicate) {
      QMessageBox::warning(this, "Notice", "This stock is already in the list");
      return;
    }

    // Validation passed, add the stock
    addedCodes.push_back(codeStr);
    refreshList();
  }
}

// Delete stock (record to deletedCodes; directly remove if it's an added stock)
void ConfigDialog::on_removeButton_clicked() {
  QList<QListWidgetItem *> selected = ui->listWidget->selectedItems();
  if (selected.isEmpty()) {
    QMessageBox::information(this, "Notice",
                             "Please select stocks to delete first");
    return;
  }

  for (auto item : selected) {
    QString text = item->text();
    std::string itemStr = text.toStdString();
    bool isAdded = itemStr.find("(new)") != std::string::npos;

    // Extract stock code (format: "code name" or "code -- (new)")
    std::string code;
    size_t firstSpace = itemStr.find(' ');
    if (firstSpace != std::string::npos) {
      code = itemStr.substr(0, firstSpace);
    }

    if (isAdded) {
      // Remove from added list
      auto it = std::remove(addedCodes.begin(), addedCodes.end(), code);
      addedCodes.erase(it, addedCodes.end());
    } else {
      // Mark original stock for deletion
      deletedCodes.insert(code);
    }
    delete item;
  }
}

// Reset button (clear deletion and addition records)
void ConfigDialog::on_resetButton_clicked() {
  deletedCodes.clear();
  addedCodes.clear(); // Clear added codes list
  refreshList();
}
