#ifdef WITH_GUI

#pragma once

#include "gui/GameWidget.hpp"
#include "poople/poople.hpp"
#include "utils/utils.hpp"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QThread>
#include <QWidget>
#include <atomic>
#include <vector>

namespace Ui {
class PoopleWidget;
}

class PoopleWidget : public GameWidget {
  Q_OBJECT

public:
  explicit PoopleWidget(QWidget *parent = nullptr);
  ~PoopleWidget() override;

public slots:
  void newGame() override;

private slots:
  void onNewGame() override;
  void onSolve();
  void onSettings() override;
  void onSolverFinished() override;
  void onStartTextChanged(const QString &text);

private:
  class SolverThread : public QThread {
  public:
    SolverThread(const Poople::Config &cfg, std::atomic<bool> *cancelFlag)
        : config(cfg), cancellationFlag(cancelFlag) {}

    Poople::SolverResult getResult() const { return result; }

  protected:
    void run() override {
      result = Poople::solve(config, *cancellationFlag);
    }

  private:
    Poople::Config config;
    Poople::SolverResult result;
    std::atomic<bool> *cancellationFlag;
  };

  Ui::PoopleWidget *ui;
  Poople::Config config;
  std::vector<Poople::Solution> solutions;

  bool showConfigDialog();
  void initGame() override;
  void setUIEnabled(bool enabled) override;
  void updateConfigInfo() override;
  void populateResults();
};

#endif // WITH_GUI
