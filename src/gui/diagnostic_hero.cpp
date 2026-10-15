#include "diagnostic_hero.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace minesleuth::gui {

DiagnosticHero::DiagnosticHero(QWidget* parent) : QFrame(parent) {
    setObjectName("HeroCard");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(10);

    // 1. Direct Headline
    m_headlineLabel = new QLabel(this);
    m_headlineLabel->setStyleSheet("font-size: 16px; font-weight: 700; color: #FFFFFF;");
    m_headlineLabel->setWordWrap(true);
    mainLayout->addWidget(m_headlineLabel);

    // 2. Explanation / Plain English
    m_explanationLabel = new QLabel(this);
    m_explanationLabel->setStyleSheet("font-size: 12px; color: #9E9E9E; line-height: 1.4;");
    m_explanationLabel->setWordWrap(true);
    mainLayout->addWidget(m_explanationLabel);

    // 3. Recommended Fix Box
    m_fixBox = new QFrame(this);
    m_fixBox->setObjectName("FixBox");
    auto* fixLayout = new QVBoxLayout(m_fixBox);
    fixLayout->setContentsMargins(12, 10, 12, 10);

    m_fixTextLabel = new QLabel(m_fixBox);
    m_fixTextLabel->setStyleSheet("font-size: 12px; color: #D4D4D4;");
    m_fixTextLabel->setWordWrap(true);
    fixLayout->addWidget(m_fixTextLabel);
    mainLayout->addWidget(m_fixBox);

    // 4. Action Buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    m_copyFixButton = new QPushButton(QIcon(":/icons/clipboard.svg"), "Copy Fix Instructions", this);
    m_copyFixButton->setObjectName("PrimaryButton");
    btnLayout->addWidget(m_copyFixButton);

    m_openConfigButton = new QPushButton(QIcon(":/icons/folder.svg"), "Open Config File", this);
    m_openConfigButton->setObjectName("SecondaryButton");
    btnLayout->addWidget(m_openConfigButton);

    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    connect(m_copyFixButton, &QPushButton::clicked, this, [this]() {
        if (!m_currentFixText.isEmpty()) {
            QGuiApplication::clipboard()->setText(m_currentFixText);
            emit copyFixInstructionsRequested(m_currentFixText);
        }
    });

    connect(m_openConfigButton, &QPushButton::clicked, this, [this]() {
        emit openConfigFileRequested(m_currentConfigPath);
    });

    clear();
}

void DiagnosticHero::setAnalysis(const AnalysisResult& result) {
    if (result.diagnostics.empty()) {
        m_headlineLabel->setText("No Critical Errors or Crashes Detected");
        m_explanationLabel->setText("The log file was analyzed across 21 known diagnostic rules. All routines executed normally.");
        m_fixBox->hide();
        m_copyFixButton->hide();
        m_openConfigButton->hide();
        return;
    }

    const auto& topDiag = result.diagnostics.front();

    // Direct Headline
    if (topDiag.mod.identified && !topDiag.mod.modId.empty()) {
        if (topDiag.mod.isPlatform()) {
            if (topDiag.ruleId == "class-not-found") {
                QString ver = QString::fromStdString(topDiag.mod.modVersion);
                m_headlineLabel->setText("Mod Incompatibility with Minecraft" + (ver.isEmpty() ? "" : " " + ver));
            } else if (topDiag.ruleId == "minecraft-version-mismatch") {
                m_headlineLabel->setText("Minecraft Version Mismatch");
            } else {
                QString name = QString::fromStdString(topDiag.mod.modName.empty() ? topDiag.mod.modId : topDiag.mod.modName);
                m_headlineLabel->setText("Platform Issue: " + name);
            }
        } else {
            QString name = QString::fromStdString(topDiag.mod.modName.empty() ? topDiag.mod.modId : topDiag.mod.modName);
            m_headlineLabel->setText(name + " caused an error or crash");
        }
    } else {
        m_headlineLabel->setText(QString::fromStdString(topDiag.title));
    }

    // Explanation
    QString explanation = QString::fromStdString(topDiag.explanation);
    if (!topDiag.evidence.empty()) {
        explanation += " Evidence: " + QString::fromStdString(topDiag.evidence);
    }
    m_explanationLabel->setText(explanation);

    // Fix / Suggestion
    std::string fix = topDiag.mod.actionStep(topDiag.ruleId);
    if (fix.empty() && !topDiag.suggestions.empty()) {
        fix = topDiag.suggestions.front();
    }

    if (!fix.empty()) {
        m_currentFixText = QString::fromStdString(fix);
        m_fixTextLabel->setText(QString("<b>Recommended Fix:</b> %1").arg(m_currentFixText));
        m_fixBox->show();
        m_copyFixButton->show();
    } else {
        m_fixBox->hide();
        m_copyFixButton->hide();
    }

    m_openConfigButton->show();
}

void DiagnosticHero::clear() {
    m_headlineLabel->setText("Drop a log file to begin analysis");
    m_explanationLabel->setText("Drag and drop latest.log or crash-report.txt into this window to detect issues.");
    m_fixBox->hide();
    m_copyFixButton->hide();
    m_openConfigButton->hide();
    m_currentFixText.clear();
    m_currentConfigPath.clear();
}

}  // namespace minesleuth::gui
