#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>

#include "models/analysis_result.h"

namespace minesleuth::gui {

class DiagnosticHero : public QFrame {
    Q_OBJECT

public:
    explicit DiagnosticHero(QWidget* parent = nullptr);

    void setAnalysis(const AnalysisResult& result);
    void clear();

signals:
    void openConfigFileRequested(const QString& path);
    void copyFixInstructionsRequested(const QString& instructions);

private:
    QLabel* m_headlineLabel = nullptr;
    QLabel* m_explanationLabel = nullptr;
    QFrame* m_fixBox = nullptr;
    QLabel* m_fixTextLabel = nullptr;
    QPushButton* m_copyFixButton = nullptr;
    QPushButton* m_openConfigButton = nullptr;

    QString m_currentFixText;
    QString m_currentConfigPath;
};

}  // namespace minesleuth::gui
