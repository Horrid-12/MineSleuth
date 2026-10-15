#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>

#include "models/analysis_result.h"

namespace minesleuth::gui {

class ActionSidebar : public QFrame {
    Q_OBJECT

public:
    explicit ActionSidebar(QWidget* parent = nullptr);

    void setAnalysis(const AnalysisResult& result, const QString& currentFilePath);
    void clear();

signals:
    void openModsFolderRequested();
    void copyCrashReportRequested();
    void uploadMclogsRequested();
    void searchModrinthRequested(const QString& modId);
    void uploadPastebinRequested();
    void viewWikiRequested(const QString& modId);

private:
    QLabel* m_modNameLabel = nullptr;
    QLabel* m_modVersionLabel = nullptr;

    QPushButton* m_openModsButton = nullptr;
    QPushButton* m_copyCrashButton = nullptr;
    QPushButton* m_uploadMclogsButton = nullptr;
    QPushButton* m_searchModrinthButton = nullptr;
    QPushButton* m_uploadPastebinButton = nullptr;
    QPushButton* m_viewWikiButton = nullptr;

    QString m_currentModId;
    QString m_currentFilePath;
};

}  // namespace minesleuth::gui
