#include "action_sidebar.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QUrl>
#include <QVBoxLayout>

namespace minesleuth::gui {

ActionSidebar::ActionSidebar(QWidget* parent) : QFrame(parent) {
    setObjectName("SidebarCard");
    setFixedWidth(220);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 16, 12, 16);
    mainLayout->setSpacing(16);

    // 1. Mod Info Header Box
    auto* modBox = new QFrame(this);
    auto* modLayout = new QHBoxLayout(modBox);
    modLayout->setContentsMargins(0, 0, 0, 12);
    modLayout->setSpacing(10);

    auto* iconLabel = new QLabel(modBox);
    iconLabel->setPixmap(QIcon(":/icons/cube.svg").pixmap(24, 24));
    modLayout->addWidget(iconLabel);

    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    m_modNameLabel = new QLabel("No Log Loaded", modBox);
    m_modNameLabel->setStyleSheet("font-weight: 600; font-size: 13px; color: #FFFFFF;");
    textLayout->addWidget(m_modNameLabel);

    m_modVersionLabel = new QLabel("—", modBox);
    m_modVersionLabel->setStyleSheet("color: #9E9E9E; font-size: 11px;");
    textLayout->addWidget(m_modVersionLabel);
    modLayout->addLayout(textLayout);

    mainLayout->addWidget(modBox);

    // 2. Action Buttons Stack
    auto* btnLayout = new QVBoxLayout();
    btnLayout->setSpacing(6);

    auto createBtn = [this](const QString& iconPath, const QString& text) {
        auto* btn = new QPushButton(QIcon(iconPath), text, this);
        btn->setStyleSheet("text-align: left; padding-left: 10px; height: 30px;");
        return btn;
    };

    m_openModsButton = createBtn(":/icons/folder.svg", "Open Mods Folder");
    btnLayout->addWidget(m_openModsButton);

    m_copyCrashButton = createBtn(":/icons/clipboard.svg", "Copy Crash Report");
    btnLayout->addWidget(m_copyCrashButton);

    m_uploadMclogsButton = createBtn(":/icons/upload.svg", "Upload to mclo.gs");
    btnLayout->addWidget(m_uploadMclogsButton);

    m_searchModrinthButton = createBtn(":/icons/globe.svg", "Search on Modrinth");
    btnLayout->addWidget(m_searchModrinthButton);

    m_uploadPastebinButton = createBtn(":/icons/upload.svg", "Upload to Pastebin");
    btnLayout->addWidget(m_uploadPastebinButton);

    m_viewWikiButton = createBtn(":/icons/clipboard.svg", "View Solution Wiki");
    btnLayout->addWidget(m_viewWikiButton);

    mainLayout->addLayout(btnLayout);
    mainLayout->addStretch();

    // Connect standard actions
    connect(m_openModsButton, &QPushButton::clicked, this, [this]() {
        if (!m_currentFilePath.isEmpty()) {
            QFileInfo fi(m_currentFilePath);
            QDesktopServices::openUrl(QUrl::fromLocalFile(fi.dir().absolutePath()));
        }
        emit openModsFolderRequested();
    });

    connect(m_searchModrinthButton, &QPushButton::clicked, this, [this]() {
        if (!m_currentModId.isEmpty()) {
            QString url = QString("https://modrinth.com/mods?q=%1").arg(m_currentModId);
            QDesktopServices::openUrl(QUrl(url));
        }
        emit searchModrinthRequested(m_currentModId);
    });

    connect(m_copyCrashButton, &QPushButton::clicked, this, &ActionSidebar::copyCrashReportRequested);
    connect(m_uploadMclogsButton, &QPushButton::clicked, this, &ActionSidebar::uploadMclogsRequested);
    connect(m_uploadPastebinButton, &QPushButton::clicked, this, &ActionSidebar::uploadPastebinRequested);
    connect(m_viewWikiButton, &QPushButton::clicked, this, [this]() {
        emit viewWikiRequested(m_currentModId);
    });

    clear();
}

void ActionSidebar::setAnalysis(const AnalysisResult& result, const QString& currentFilePath) {
    m_currentFilePath = currentFilePath;

    if (!result.diagnostics.empty() && result.diagnostics.front().mod.identified) {
        const auto& modAttr = result.diagnostics.front().mod;
        m_currentModId = QString::fromStdString(modAttr.modId);
        if (m_currentModId.isEmpty()) {
            m_currentModId = QString::fromStdString(modAttr.ownerPackage);
        }

        if (modAttr.isPlatform()) {
            QString name = QString::fromStdString(modAttr.modName.empty() ? modAttr.modId : modAttr.modName);
            m_modNameLabel->setText(name);
            QString ver = QString::fromStdString(modAttr.modVersion);
            m_modVersionLabel->setText(ver.isEmpty() ? "Base Game (Platform)" : ver + " (Platform)");
            m_searchModrinthButton->setEnabled(false);
            m_viewWikiButton->setEnabled(false);
        } else {
            m_modNameLabel->setText(m_currentModId);
            bool foundVersion = false;
            if (result.crashReport.has_value()) {
                for (const auto& mod : result.crashReport->mods) {
                    if (mod.id == modAttr.modId) {
                        m_modVersionLabel->setText(QString::fromStdString(mod.version));
                        foundVersion = true;
                        break;
                    }
                }
            }
            if (!foundVersion) {
                m_modVersionLabel->setText("Culprit Mod");
            }
            m_searchModrinthButton->setEnabled(!m_currentModId.isEmpty());
            m_viewWikiButton->setEnabled(!m_currentModId.isEmpty());
        }
    } else {
        m_currentModId.clear();
        m_modNameLabel->setText("Vanilla / Core");
        m_modVersionLabel->setText("No mod culprit");
        m_searchModrinthButton->setEnabled(false);
        m_viewWikiButton->setEnabled(false);
    }

    m_openModsButton->setEnabled(true);
    m_copyCrashButton->setEnabled(true);
    m_uploadMclogsButton->setEnabled(true);
    m_uploadPastebinButton->setEnabled(true);
}

void ActionSidebar::clear() {
    m_currentModId.clear();
    m_currentFilePath.clear();
    m_modNameLabel->setText("No Log Loaded");
    m_modVersionLabel->setText("—");

    m_openModsButton->setEnabled(false);
    m_copyCrashButton->setEnabled(false);
    m_uploadMclogsButton->setEnabled(false);
    m_searchModrinthButton->setEnabled(false);
    m_uploadPastebinButton->setEnabled(false);
    m_viewWikiButton->setEnabled(false);
}

}  // namespace minesleuth::gui
