#pragma once

#include <QSortFilterProxyModel>
#include <QSet>

class FilterProxyModel : public QSortFilterProxyModel {
public:
    explicit FilterProxyModel(QObject* parent = nullptr) 
        : QSortFilterProxyModel(parent), m_filterEnabled(false) {
        setDynamicSortFilter(false);
    }
    
    void setFilterRows(const QSet<int>& rows) {
        m_filterEnabled = !rows.isEmpty();
        m_filteredRows = rows;
        invalidateFilter();
    }
    
    void clearFilter() {
        m_filterEnabled = false;
        m_filteredRows.clear();
        invalidateFilter();
    }
    
    bool isFilterEnabled() const { return m_filterEnabled; }
    
    int mapFromSourceRow(int sourceRow) const {
        if (!m_filterEnabled) return sourceRow;
        return mapFromSource(sourceModel()->index(sourceRow, 0)).row();
    }
    
    int mapToSourceRow(int proxyRow) const {
        return mapToSource(index(proxyRow, 0)).row();
    }
    
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        Q_UNUSED(sourceParent)
        if (!m_filterEnabled) return true;
        return m_filteredRows.contains(sourceRow);
    }
    
private:
    bool m_filterEnabled;
    QSet<int> m_filteredRows;
};
