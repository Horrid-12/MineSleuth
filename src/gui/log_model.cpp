#include "log_model.h"

#include <QFont>
#include <QString>

namespace minesleuth::gui {

namespace {

QColor levelColor(LogLevel level) {
    switch (level) {
        case LogLevel::FATAL:
        case LogLevel::ERROR:
            return QColor("#E05252");
        case LogLevel::WARN:
            return QColor("#D4A347");
        case LogLevel::INFO:
            return QColor("#E0E0E0");
        case LogLevel::DEBUG:
        case LogLevel::TRACE:
        case LogLevel::UNKNOWN:
            return QColor("#707070");
    }
    return QColor("#E0E0E0");
}

}  // namespace

LogModel::LogModel(QObject* parent) : QAbstractTableModel(parent) {}

void LogModel::setEntries(std::vector<LogEntry> entries) {
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

void LogModel::clear() {
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

int LogModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

int LogModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return ColCount;
}

QVariant LogModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size())) {
        return {};
    }

    const auto& entry = m_entries[static_cast<std::size_t>(index.row())];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColLine:
                return static_cast<qlonglong>(entry.line_number);
            case ColTime:
                return QString::fromStdString(entry.timestamp);
            case ColLevel:
                return QString::fromStdString(minesleuth::toString(entry.level));
            case ColThread:
                return QString::fromStdString(entry.thread);
            case ColMessage:
                return QString::fromStdString(entry.message);
        }
    } else if (role == Qt::ForegroundRole) {
        if (index.column() == ColLevel) {
            return levelColor(entry.level);
        }
        if (entry.level == LogLevel::FATAL || entry.level == LogLevel::ERROR) {
            return QColor("#E06C75");
        }
        if (entry.level == LogLevel::WARN) {
            return QColor("#D4A347");
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColLine) {
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    return {};
}

QVariant LogModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
        case ColLine: return "Line";
        case ColTime: return "Time";
        case ColLevel: return "Level";
        case ColThread: return "Thread";
        case ColMessage: return "Message";
    }

    return {};
}

const LogEntry* LogModel::getEntry(int row) const {
    if (row < 0 || row >= static_cast<int>(m_entries.size())) {
        return nullptr;
    }
    return &m_entries[static_cast<std::size_t>(row)];
}

}  // namespace minesleuth::gui
