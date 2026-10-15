#pragma once

#include <QAbstractTableModel>
#include <QColor>
#include <vector>

#include "models/log_entry.h"

namespace minesleuth::gui {

class LogModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColLine = 0,
        ColTime,
        ColLevel,
        ColThread,
        ColMessage,
        ColCount
    };

    explicit LogModel(QObject* parent = nullptr);

    void setEntries(std::vector<LogEntry> entries);
    void clear();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    const LogEntry* getEntry(int row) const;

private:
    std::vector<LogEntry> m_entries;
};

}  // namespace minesleuth::gui
