#ifndef WIDGET_H
#define WIDGET_H

#include "data.h"
#include <QAction>
#include <QMenu>
#include <QMouseEvent>
#include <QWidget>

class Widget : public QWidget {
  Q_OBJECT

public:
  explicit Widget(QWidget *parent = nullptr);
  ~Widget() override;

private:
  void setScaledSize();
  void drawLineChart(QPainter *painter, const QColor &color, int startX,
                     int width, int height);
  void drawTextNumbers(QPainter *painter, const QColor &color, int areaWidth,
                       int areaHeight);
  void updateWindowSize(); // Update window size

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void
  contextMenuEvent(QContextMenuEvent *event) override; // Right-click menu event

private slots:
  void onDataUpdated();
  void onShowLineChart(); // Show line chart
  void onShowOnlyData();  // Show data only
  void onExit();

private:
  DataManager *dataManager;
  bool m_dragging;
  QPoint m_dragStartPosition;
  bool m_showLineChart;      // Flag for showing line chart
  QAction *m_actionShowLine; // Menu item for showing line chart
  QAction *m_actionShowData; // Menu item for showing data only
  QAction *m_actionExit;
};

#endif // WIDGET_H
