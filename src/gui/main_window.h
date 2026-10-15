#pragma once

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <memory>
#include <string>

#include "action_sidebar.h"
#include "analyzer/log_analyzer.h"
#include "diagnostic_hero.h"
#include "log_model.h"

class QDragEnterEvent;
class QDropEvent;
class QLineEdit;
class QLabel;

namespace minesleuth::gui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

    void openFile(const QString& filePath);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onOpenLogClicked();
    void onClearClicked();
    void onExportClicked();
    void onSearchTextChanged(const QString& text);

private:
    void setupUi();
    void setupToolBar();
    void setupCentralArea();
    void setupStatusBar();
    void applyAnalysisResult(const AnalysisResult& result, const QString& filePath);
    void populateSummaryTab(const AnalysisResult& result);
    void populateStackTraceTab(const AnalysisResult& result);
    void populateModsTab(const AnalysisResult& result);

    // Core analysis engine
    LogAnalyzer m_analyzer;

    // UI Widgets
    QLineEdit* m_searchEdit = nullptr;
    QLabel* m_sessionLabel = nullptr;

    DiagnosticHero* m_heroCard = nullptr;
    QTabWidget* m_tabWidget = nullptr;
    QTableView* m_rawLogTable = nullptr;
    QPlainTextEdit* m_stackTraceView = nullptr;
    QWidget* m_summaryWidget = nullptr;
    QTableView* m_modsTable = nullptr;

    ActionSidebar* m_sidebar = nullptr;

    // Status bar labels
    QLabel* m_statusReadyLabel = nullptr;
    QLabel* m_statusDetailsLabel = nullptr;

    // Data models
    LogModel* m_logModel = nullptr;
    QSortFilterProxyModel* m_proxyModel = nullptr;

    QString m_currentFilePath;
    AnalysisResult m_currentResult;
};

}  // namespace minesleuth::gui
