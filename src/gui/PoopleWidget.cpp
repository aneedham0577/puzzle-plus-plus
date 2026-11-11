#ifdef WITH_GUI

#include "gui/PoopleWidget.hpp"
#include "ui_PoopleWidget.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>

PoopleWidget::PoopleWidget(QWidget *parent)
    : GameWidget(parent), ui(new Ui::PoopleWidget) {
  ui->setupUi(this);

  // Set up results table
  ui->resultsTable->setColumnCount(4);
  QStringList headers;
  headers << "#" << "Words" << "Min Score" << "Ladder Path";
  ui->resultsTable->setHorizontalHeaderLabels(headers);
  ui->resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  ui->resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  ui->resultsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  ui->resultsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

  // Connect UI signals
  connect(ui->solveButton, &QPushButton::clicked, this, &PoopleWidget::onSolve);
  connect(ui->newGameButton, &QPushButton::clicked, this, &PoopleWidget::onNewGame);
  connect(ui->settingsButton, &QPushButton::clicked, this, &PoopleWidget::onSettings);
  connect(ui->startLineEdit, &QLineEdit::returnPressed, this, &PoopleWidget::onSolve);
  connect(ui->endLineEdit, &QLineEdit::returnPressed, this, &PoopleWidget::onSolve);
  connect(ui->startLineEdit, &QLineEdit::textChanged, this, &PoopleWidget::onStartTextChanged);

  initGame();
}

PoopleWidget::~PoopleWidget() {
  cleanupThread();
  delete ui;
}

void PoopleWidget::initGame() {
  config.startWord = "";
  config.endWord = "poop";
  config.minWords = 2;
  config.maxWords = 10;
  config.excludeUncommonWords = true;
  config.maxSolutions = 1000;

  ui->startLineEdit->clear();
  ui->endLineEdit->setText("POOP");
  solutions.clear();
  ui->resultsTable->setRowCount(0);

  updateConfigInfo();
  setUIEnabled(true);
}

void PoopleWidget::newGame() {
  onNewGame();
}

void PoopleWidget::onNewGame() {
  initGame();
  ui->startLineEdit->setFocus();
}

void PoopleWidget::onStartTextChanged(const QString &text) {
  QString clean = text.trimmed();
  if (clean.length() == 4 && ui->endLineEdit->text().isEmpty()) {
    ui->endLineEdit->setText("POOP");
  }
}

void PoopleWidget::updateConfigInfo() {
  QString info = QString("Words: %1-%2 | Dictionary: %3 | Max Solutions: %4")
                     .arg(config.minWords)
                     .arg(config.maxWords)
                     .arg(config.excludeUncommonWords ? "Common words" : "All words")
                     .arg(config.maxSolutions);
  ui->configInfoLabel->setText(info);
}

void PoopleWidget::setUIEnabled(bool enabled) {
  ui->startLineEdit->setEnabled(enabled);
  ui->endLineEdit->setEnabled(enabled);
  ui->solveButton->setEnabled(enabled);
  ui->newGameButton->setEnabled(enabled);
  ui->settingsButton->setEnabled(enabled);
}

void PoopleWidget::onSolve() {
  QString startStr = ui->startLineEdit->text().trimmed().toLower();
  QString endStr = ui->endLineEdit->text().trimmed().toLower();

  if (startStr.isEmpty()) {
    QMessageBox::warning(this, "Input Required", "Please enter a start word.");
    ui->startLineEdit->setFocus();
    return;
  }

  if (endStr.isEmpty()) {
    if (startStr.length() == 4) {
      endStr = "poop";
      ui->endLineEdit->setText("POOP");
    } else {
      QMessageBox::warning(this, "Input Required", "Please enter a target word.");
      ui->endLineEdit->setFocus();
      return;
    }
  }

  if (startStr.length() != endStr.length()) {
    QMessageBox::warning(this, "Length Mismatch",
                         "Start word and target word must have the exact same number of letters.");
    return;
  }

  config.startWord = startStr.toStdString();
  config.endWord = endStr.toStdString();

  setUIEnabled(false);
  createProgressDialog("Finding word ladder paths...", 0, 0);

  cleanupThread();
  cancellationFlag.store(false);

  solverThread = new SolverThread(config, &cancellationFlag);
  connect(solverThread, &QThread::finished, this, &PoopleWidget::onSolverFinished);
  solverThread->start();
}

void PoopleWidget::onSolverFinished() {
  cleanupProgressDialog();

  auto *thread = qobject_cast<SolverThread *>(solverThread);
  if (thread) {
    Poople::SolverResult result = thread->getResult();
    if (!result.errorMessage.empty()) {
      QMessageBox::critical(this, "Solver Error", QString::fromStdString(result.errorMessage));
      solutions.clear();
    } else {
      solutions = result.solutions;
    }
    populateResults();
  }

  setUIEnabled(true);
}

void PoopleWidget::populateResults() {
  ui->resultsTable->setRowCount(0);
  if (solutions.empty()) {
    ui->resultsTable->setRowCount(1);
    QTableWidgetItem *item = new QTableWidgetItem("No ladder solutions found.");
    ui->resultsTable->setItem(0, 3, item);
    return;
  }

  ui->resultsTable->setRowCount(static_cast<int>(solutions.size()));
  for (size_t i = 0; i < solutions.size(); ++i) {
    const auto &sol = solutions[i];
    ui->resultsTable->setItem(static_cast<int>(i), 0, new QTableWidgetItem(QString::number(i + 1)));
    ui->resultsTable->setItem(static_cast<int>(i), 1, new QTableWidgetItem(QString::number(sol.words.size())));
    ui->resultsTable->setItem(static_cast<int>(i), 2, new QTableWidgetItem(QString::number(sol.minScore, 'f', 4)));

    QString pathStr;
    for (size_t w = 0; w < sol.words.size(); ++w) {
      if (w > 0) pathStr += " -> ";
      pathStr += QString::fromStdString(sol.words[w]).toUpper();
    }
    ui->resultsTable->setItem(static_cast<int>(i), 3, new QTableWidgetItem(pathStr));
  }
}

void PoopleWidget::onSettings() {
  if (showConfigDialog()) {
    updateConfigInfo();
  }
}

bool PoopleWidget::showConfigDialog() {
  QDialog dialog(this);
  dialog.setWindowTitle("Poople Solver Settings");

  QFormLayout *layout = new QFormLayout(&dialog);

  QSpinBox *minWordsSpin = new QSpinBox(&dialog);
  minWordsSpin->setRange(2, 20);
  minWordsSpin->setValue(config.minWords);
  layout->addRow("Min Ladder Words:", minWordsSpin);

  QSpinBox *maxWordsSpin = new QSpinBox(&dialog);
  maxWordsSpin->setRange(2, 20);
  maxWordsSpin->setValue(config.maxWords);
  layout->addRow("Max Ladder Words:", maxWordsSpin);

  QCheckBox *excludeBox = new QCheckBox("Exclude uncommon words", &dialog);
  excludeBox->setChecked(config.excludeUncommonWords);
  layout->addRow(excludeBox);

  QSpinBox *maxSolSpin = new QSpinBox(&dialog);
  maxSolSpin->setRange(10, 10000);
  maxSolSpin->setValue(config.maxSolutions);
  layout->addRow("Max Solutions Limit:", maxSolSpin);

  QDialogButtonBox *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
  layout->addRow(buttonBox);

  connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() == QDialog::Accepted) {
    config.minWords = static_cast<uint32_t>(minWordsSpin->value());
    config.maxWords = static_cast<uint32_t>(maxWordsSpin->value());
    if (config.minWords > config.maxWords) {
      std::swap(config.minWords, config.maxWords);
    }
    config.excludeUncommonWords = excludeBox->isChecked();
    config.maxSolutions = static_cast<uint32_t>(maxSolSpin->value());
    return true;
  }
  return false;
}

#endif // WITH_GUI
