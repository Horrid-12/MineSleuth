#include "main_window.h"

#include <QApplication>
#include <QClipboard>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QStandardItemModel>
#include <QToolBar>
#include <QVBoxLayout>

namespace minesleuth::gui {

namespace {

QString formatStackTrace(const StackTrace& trace) {
    if (!trace.rawText.empty()) {
        return QString::fromStdString(trace.rawText);
    }
    QString out;
    for (const auto& seg : trace.segments) {
        if (!out.isEmpty()) out += "\n";
        if (seg.kind == TraceSegmentKind::Cause) {
            out += "Caused by: ";
        } else if (seg.kind == TraceSegmentKind::Suppressed) {
            out += "Suppressed: ";
        }
        out += QString::fromStdString(seg.header()) + "\n";
        for (const auto& f : seg.frames) {
            out += "    at " + QString::fromStdString(f.raw.empty() ? f.signature() : f.raw) + "\n";
        }
        if (seg.omittedFrames > 0) {
            out += QString("    ... %1 more\n").arg(seg.omittedFrames);
        }
    }
    return out;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("MineSleuth — Minecraft Log Analyzer");
    resize(1280, 800);
    setMinimumSize(960, 600);
    setAcceptDrops(true);

    setupUi();
}

void MainWindow::setupUi() {
    setupToolBar();
    setupCentralArea();
    setupStatusBar();
}

void MainWindow::setupToolBar() {
    auto* toolbar = addToolBar("Main Navigation");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(16, 16));

    // Brand Label
    auto* brandWidget = new QWidget(toolbar);
    auto* brandLayout = new QHBoxLayout(brandWidget);
    brandLayout->setContentsMargins(4, 2, 12, 2);
    brandLayout->setSpacing(6);

    auto* brandIcon = new QLabel(brandWidget);
    brandIcon->setPixmap(QIcon(":/icons/terminal.svg").pixmap(16, 16));
    brandLayout->addWidget(brandIcon);

    auto* brandText = new QLabel("MineSleuth", brandWidget);
    brandText->setStyleSheet("font-weight: 700; font-size: 13px; color: #FFFFFF;");
    brandLayout->addWidget(brandText);
    toolbar->addWidget(brandWidget);

    // Actions
    auto* openBtn = new QPushButton(QIcon(":/icons/folder.svg"), "Open Log", toolbar);
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::onOpenLogClicked);
    toolbar->addWidget(openBtn);

    auto* clearBtn = new QPushButton("Clear", toolbar);
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClearClicked);
    toolbar->addWidget(clearBtn);

    auto* exportBtn = new QPushButton(QIcon(":/icons/upload.svg"), "Export", toolbar);
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::onExportClicked);
    toolbar->addWidget(exportBtn);

    toolbar->addSeparator();

    // Spacer
    auto* spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    // Active session file pill
    m_sessionLabel = new QLabel("No file loaded", toolbar);
    m_sessionLabel->setStyleSheet("color: #9E9E9E; font-size: 11px; margin-right: 12px;");
    toolbar->addWidget(m_sessionLabel);

    // Search bar
    m_searchEdit = new QLineEdit(toolbar);
    m_searchEdit->setPlaceholderText("Filter logs (regex, thread, mod)...");
    m_searchEdit->setFixedWidth(220);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    toolbar->addWidget(m_searchEdit);
}

void MainWindow::setupCentralArea() {
    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName("CentralWidget");
    setCentralWidget(centralWidget);

    auto* rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setContentsMargins(16, 16, 0, 16);
    rootLayout->setSpacing(16);

    // Main Left Column
    auto* mainColLayout = new QVBoxLayout();
    mainColLayout->setSpacing(14);

    // 1. Diagnostic Hero Card
    m_heroCard = new DiagnosticHero(centralWidget);
    mainColLayout->addWidget(m_heroCard);

    // 2. Tabbed Content
    m_tabWidget = new QTabWidget(centralWidget);

    // Tab 1: Summary Widget
    m_summaryWidget = new QWidget();
    auto* summaryLayout = new QVBoxLayout(m_summaryWidget);
    summaryLayout->setContentsMargins(12, 12, 12, 12);
    m_tabWidget->addTab(m_summaryWidget, "Summary");

    // Tab 2: Full Stack Trace
    m_stackTraceView = new QPlainTextEdit(m_tabWidget);
    m_stackTraceView->setObjectName("TerminalView");
    m_stackTraceView->setReadOnly(true);
    m_tabWidget->addTab(m_stackTraceView, "Full Stack Trace");

    // Tab 3: Raw Log Table View
    m_rawLogTable = new QTableView(m_tabWidget);
    m_logModel = new LogModel(this);
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_logModel);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1);  // Filter all columns

    m_rawLogTable->setModel(m_proxyModel);
    m_rawLogTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rawLogTable->setAlternatingRowColors(true);
    m_rawLogTable->verticalHeader()->setVisible(false);
    m_rawLogTable->horizontalHeader()->setStretchLastSection(true);
    m_tabWidget->addTab(m_rawLogTable, "Raw Log");

    // Tab 4: Installed Mods
    m_modsTable = new QTableView(m_tabWidget);
    m_modsTable->setAlternatingRowColors(true);
    m_modsTable->verticalHeader()->setVisible(false);
    m_modsTable->horizontalHeader()->setStretchLastSection(true);
    m_tabWidget->addTab(m_modsTable, "Installed Mods");

    mainColLayout->addWidget(m_tabWidget);
    rootLayout->addLayout(mainColLayout, 1);

    // 3. Right Action Sidebar
    m_sidebar = new ActionSidebar(centralWidget);
    rootLayout->addWidget(m_sidebar);
}

void MainWindow::setupStatusBar() {
    auto* status = statusBar();
    m_statusReadyLabel = new QLabel("Ready", this);
    m_statusDetailsLabel = new QLabel("No log loaded", this);

    status->addWidget(m_statusReadyLabel);
    status->addPermanentWidget(m_statusDetailsLabel);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            event->acceptProposedAction();
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const auto urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString localPath = urls.first().toLocalFile();
        if (!localPath.isEmpty()) {
            openFile(localPath);
            event->acceptProposedAction();
        }
    }
}

void MainWindow::onOpenLogClicked() {
    QString path = QFileDialog::getOpenFileName(
        this,
        "Open Minecraft Log or Crash Report",
        QString(),
        "Minecraft Files (*.log *.txt);;Log Files (*.log);;Crash Reports (*.txt);;All Files (*)"
    );

    if (!path.isEmpty()) {
        openFile(path);
    }
}

void MainWindow::openFile(const QString& filePath) {
    m_currentFilePath = filePath;
    QFileInfo fi(filePath);

    m_statusReadyLabel->setText("Analyzing " + fi.fileName() + "...");
    QApplication::processEvents();

    m_currentResult = m_analyzer.analyzeFile(filePath.toStdString());
    applyAnalysisResult(m_currentResult, filePath);
}

void MainWindow::applyAnalysisResult(const AnalysisResult& result, const QString& filePath) {
    QFileInfo fi(filePath);
    m_sessionLabel->setText(QString("%1 • %2").arg(fi.fileName(), minesleuth::toString(result.format)));

    // Hero & Sidebar
    m_heroCard->setAnalysis(result);
    m_sidebar->setAnalysis(result, filePath);

    // Raw log table
    m_logModel->setEntries(result.entries);

    // Tabs population
    populateSummaryTab(result);
    populateStackTraceTab(result);
    populateModsTab(result);

    // Status bar update
    m_statusReadyLabel->setText(QString("Ready • %1 issues identified").arg(result.diagnostics.size()));
    m_statusDetailsLabel->setText(QString("%1 lines parsed | Format: %2")
        .arg(result.statistics.total_entries)
        .arg(minesleuth::toString(result.format)));
}

void MainWindow::populateSummaryTab(const AnalysisResult& result) {
    // Clear existing layout items
    QLayoutItem* item;
    while ((item = m_summaryWidget->layout()->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    auto* layout = qobject_cast<QVBoxLayout*>(m_summaryWidget->layout());
    if (!layout) return;

    if (result.diagnostics.empty() && result.groups.empty()) {
        auto* label = new QLabel("No issues found in this log session.", m_summaryWidget);
        label->setStyleSheet("color: #9E9E9E; font-size: 13px;");
        layout->addWidget(label);
        layout->addStretch();
        return;
    }

    // List Diagnostics
    for (const auto& diag : result.diagnostics) {
        auto* frame = new QFrame(m_summaryWidget);
        frame->setStyleSheet("background-color: #202020; border: 1px solid #333333; border-radius: 2px; padding: 8px; margin-bottom: 4px;");
        auto* fLayout = new QVBoxLayout(frame);
        fLayout->setSpacing(4);

        auto* title = new QLabel(QString::fromStdString(diag.title), frame);
        title->setStyleSheet("font-weight: 600; color: #FFFFFF; font-size: 12px;");
        title->setWordWrap(true);
        fLayout->addWidget(title);

        auto* desc = new QLabel(QString::fromStdString(diag.explanation), frame);
        desc->setStyleSheet("color: #9E9E9E; font-size: 11px;");
        desc->setWordWrap(true);
        fLayout->addWidget(desc);

        layout->addWidget(frame);
    }

    layout->addStretch();
}

void MainWindow::populateStackTraceTab(const AnalysisResult& result) {
    QString text;
    if (result.crashReport.has_value() && !result.crashReport->exceptions.empty()) {
        for (const auto& trace : result.crashReport->exceptions) {
            text += formatStackTrace(trace) + "\n\n";
        }
    } else if (!result.exceptions.empty()) {
        for (const auto& occ : result.exceptions) {
            text += QString("--- Exception at log line %1 ---\n").arg(occ.entryIndex);
            text += formatStackTrace(occ.trace) + "\n\n";
        }
    } else {
        text = "No Java exception stack traces found in this session.";
    }

    m_stackTraceView->setPlainText(text);
}

void MainWindow::populateModsTab(const AnalysisResult& result) {
    auto* model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({"Mod ID", "Version", "Name", "Source"});

    if (result.crashReport.has_value()) {
        for (const auto& mod : result.crashReport->mods) {
            QList<QStandardItem*> row;
            row.append(new QStandardItem(QString::fromStdString(mod.id)));
            row.append(new QStandardItem(QString::fromStdString(mod.version)));
            row.append(new QStandardItem(QString::fromStdString(mod.name)));
            row.append(new QStandardItem("Crash Report"));
            model->appendRow(row);
        }
    }

    m_modsTable->setModel(model);
    m_tabWidget->setTabText(3, QString("Installed Mods (%1)").arg(model->rowCount()));
}

void MainWindow::onClearClicked() {
    m_currentFilePath.clear();
    m_sessionLabel->setText("No file loaded");
    m_heroCard->clear();
    m_sidebar->clear();
    m_logModel->clear();
    m_stackTraceView->clear();
    m_statusReadyLabel->setText("Ready");
    m_statusDetailsLabel->setText("No log loaded");
}

void MainWindow::onExportClicked() {
    if (m_currentFilePath.isEmpty()) return;

    QString savePath = QFileDialog::getSaveFileName(
        this,
        "Export Analysis Report",
        "minesleuth-report.txt",
        "Text Report (*.txt);;JSON Data (*.json)"
    );

    if (!savePath.isEmpty()) {
        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "=== MineSleuth Analysis Report ===\n";
            out << "File: " << m_currentFilePath << "\n\n";
            for (const auto& diag : m_currentResult.diagnostics) {
                out << "[" << minesleuth::toString(diag.severity) << "] "
                    << QString::fromStdString(diag.title) << "\n";
                out << "Explanation: " << QString::fromStdString(diag.explanation) << "\n";
                if (!diag.suggestions.empty()) {
                    out << "Suggestion: " << QString::fromStdString(diag.suggestions.front()) << "\n";
                }
                out << "\n";
            }
        }
    }
}

void MainWindow::onSearchTextChanged(const QString& text) {
    m_proxyModel->setFilterRegularExpression(text);
}

}  // namespace minesleuth::gui
