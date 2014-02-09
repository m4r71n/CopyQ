/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CLIPBOARDMODEL_H
#define CLIPBOARDMODEL_H

#include "item/clipboarditem.h"

#include <QAbstractListModel>
#include <QList>
#include <QSharedPointer>

class QMimeData;

/**
 * Model containing ClipboardItem objects.
 *
 * Class implements QAbstractListModel interface.
 *
 * Clipboard item in model can be serialized and deserialized using
 * operators << and >> (see @ref clipboard_model_serialization_operators).
 */
class ClipboardModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int maxItems READ maxItems WRITE setMaxItems)
    Q_PROPERTY(bool disabled READ isDisabled WRITE setDisabled)
    Q_PROPERTY(QString tabName READ tabName WRITE setTabName NOTIFY tabNameChanged)

public:
    /** Argument type for comparison. First is row, second is item pointer. */
    typedef QPair<int, ClipboardItemPtr> ComparisonItem;

    /** Return true if lhs is greater than rhs. */
    typedef bool CompareItems(const ComparisonItem &lhs,
                              const ComparisonItem &rhs);

    explicit ClipboardModel(QObject *parent = NULL);

    ~ClipboardModel();

    /** Return number of items in model. */
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    /** Return data for given @a index. */
    QVariant data(const QModelIndex &index, int role) const;

    /** Return item data for editing. */
    QVariant data(int row) const;

    /** Return data in given @a row.  */
    QVariantMap dataMapInRow(int row) const;

    /** Return item in given @a row.  */
    ClipboardItemPtr at(int row) const;

    /** Return flags for given @a index. */
    Qt::ItemFlags flags(const QModelIndex &index) const;

    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole);
    bool insertRows(int position, int rows,
                    const QModelIndex &index = QModelIndex());
    bool removeRows(int position, int rows,
                    const QModelIndex &index = QModelIndex());

    /** Set data for given @a index. */
    bool setDataMap(const QModelIndex &index, const QVariantMap &value);

    /** insert new item to model. */
    void insertItem(const QVariantMap &data, int row);

    /**
     * Set maximum number of items in model.
     *
     * If there are too many items last item is removed until @a max is less or
     * equal to number of items in model.
     */
    void setMaxItems(int max);

    /** Return maximum number of items in model. */
    int maxItems() const { return m_max; }

    /** Disabled model shouldn't be changed until loaded. */
    bool isDisabled() const { return m_disabled; }

    void setDisabled(bool disabled) { m_disabled = disabled; }

    /** Tab name associated with model. */
    const QString &tabName() const { return m_tabName; }

    void setTabName(const QString &tabName);

    /**
     * Move an item.
     * @return True only if item was successfully moved.
     */
    bool move(
            int pos, //!< Source row number.
            int newpos //!< Destination row number.
            );
    /**
     * Move items.
     * @return True only if all items was successfully moved.
     */
    bool moveItemsWithKeyboard(
            QModelIndexList list, //!< Indexed of items to move.
            int key,
            /*!< Key representing direction for movement (can be one of
             *   Qt::Key_Down, Qt::Key_Up, Qt::Key_End, Qt::Key_Home).
             */
            int count = 1
            );

    /**
     * Sort items in ascending order.
     */
    void sortItems(const QModelIndexList &indexList, CompareItems *compare);

    /**
     * Find item with given @a hash.
     * @return Row number with found item or -1 if no item was found.
     */
    int findItem(uint hash) const;

    /**
     * Return row index for given @a row.
     * @return Value of @a row if such index is in model.
     * @return -1 if model is empty.
     * @return 0 if @a cycle is true and @a row is bigger than last row index.
     * @return 0 if @a cycle is false and @a row is negative.
     * @return Last row index if @a cycle is false and @a row is bigger than last row index.
     * @return Last row index if @a cycle is true and @a row is negative.
     */
    int getRowNumber(int row, bool cycle = false) const;

    /** Emit unloaded() and unload (remove) all items. */
    void unloadItems();

signals:
    void unloaded();
    void tabNameChanged(const QString &tabName);

private:
    QList<ClipboardItemPtr> m_clipboardList;
    int m_max;
    bool m_disabled;
    QString m_tabName;
};

#endif // CLIPBOARDMODEL_H
