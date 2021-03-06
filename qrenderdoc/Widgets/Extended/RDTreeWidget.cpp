/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "RDTreeWidget.h"
#include <QColor>
#include <QDebug>
#include <QMouseEvent>

class RDTreeWidgetModel : public QAbstractItemModel
{
public:
  RDTreeWidgetModel(RDTreeWidget *parent) : QAbstractItemModel(parent), widget(parent) {}
  QModelIndex indexForItem(RDTreeWidgetItem *item, int column) const
  {
    if(item->m_parent == NULL)
      return QModelIndex();

    int row = item->m_parent->indexOfChild(item);

    return createIndex(row, column, item);
  }

  RDTreeWidgetItem *itemForIndex(QModelIndex idx) const
  {
    if(!idx.isValid())
      return widget->m_root;

    return (RDTreeWidgetItem *)idx.internalPointer();
  }

  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    if(row < 0 || column < 0 || row >= rowCount(parent) || column >= columnCount(parent))
      return QModelIndex();

    RDTreeWidgetItem *par = itemForIndex(parent);

    if(par == NULL)
      par = widget->m_root;

    return createIndex(row, column, par->m_children[row]);
  }

  void beginAddChild(RDTreeWidgetItem *item)
  {
    QModelIndex index = indexForItem(item, 0);
    beginInsertRows(index, item->childCount(), item->childCount());
  }

  void endAddChild(RDTreeWidgetItem *item) { endInsertRows(); }
  void beginRemoveChildren(RDTreeWidgetItem *parent, int first, int last)
  {
    beginRemoveRows(indexForItem(parent, 0), first, last);
  }

  void endRemoveChildren() { endRemoveRows(); }
  void itemChanged(RDTreeWidgetItem *item, const QVector<int> &roles)
  {
    QModelIndex topLeft = indexForItem(item, 0);
    QModelIndex bottomRight = indexForItem(item, columnCount() - 1);
    emit dataChanged(topLeft, bottomRight, roles);
  }

  void refresh()
  {
    emit beginResetModel();
    emit endResetModel();
  }

  void headerRefresh() { emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1); }
  void itemsChanged(RDTreeWidgetItem *p, QPair<int, int> minRowColumn, QPair<int, int> maxRowColumn,
                    const QVector<int> &roles)
  {
    QModelIndex topLeft = createIndex(minRowColumn.first, minRowColumn.second, p);
    QModelIndex bottomRight = createIndex(maxRowColumn.first, maxRowColumn.second, p);
    emit dataChanged(topLeft, bottomRight, roles);
  }

  QModelIndex parent(const QModelIndex &index) const override
  {
    if(index.internalPointer() == NULL)
      return QModelIndex();

    RDTreeWidgetItem *item = itemForIndex(index);

    if(item)
      return indexForItem(item->m_parent, 0);

    return QModelIndex();
  }
  int rowCount(const QModelIndex &parent = QModelIndex()) const override
  {
    if(!parent.isValid())
      return widget->m_root->childCount();

    RDTreeWidgetItem *parentItem = itemForIndex(parent);

    if(parentItem)
      return parentItem->childCount();
    return 0;
  }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override
  {
    return widget->m_headers.count();
  }

  bool hasChildren(const QModelIndex &parent) const override
  {
    if(!parent.isValid())
      return widget->m_root->childCount() > 0;

    RDTreeWidgetItem *parentItem = itemForIndex(parent);

    if(parentItem)
      return parentItem->childCount() > 0;
    return false;
  }
  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return 0;

    return QAbstractItemModel::flags(index);
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole && section < widget->m_headers.count())
      return widget->m_headers[section];

    return QVariant();
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    RDTreeWidgetItem *item = itemForIndex(index);

    // invisible root element has no data
    if(!item->m_parent)
      return QVariant();

    if(role == Qt::DisplayRole)
    {
      if(index.column() < item->m_text.count())
        return item->m_text[index.column()];
    }
    else if(role == Qt::DecorationRole)
    {
      if(widget->m_hoverColumn == index.column())
      {
        if(widget->m_currentHoverItem == item)
          return widget->m_activeHoverIcon;
        else
          return widget->m_normalHoverIcon;
      }

      if(index.column() < item->m_icons.count())
        return item->m_icons[index.column()];
    }
    else if(role == Qt::BackgroundRole)
    {
      // item's background color takes priority
      if(item->m_back != QBrush())
        return item->m_back;

      // otherwise if we're hover-highlighting, use the window color
      if(widget->m_currentHoverItem == item)
        return widget->palette().brush(QPalette::Window);

      // otherwise, no special background
      return QBrush();
    }
    else if(role == Qt::ForegroundRole)
    {
      // same priority order as background role above
      if(item->m_fore != QBrush())
        return item->m_fore;

      if(widget->m_currentHoverItem == item)
        return widget->palette().brush(QPalette::WindowText);

      return QBrush();
    }
    else if(role == Qt::ToolTipRole)
    {
      return item->m_tooltip;
    }
    else if(role == Qt::FontRole)
    {
      QFont font;    // TODO should this come from some default?
      font.setItalic(item->m_italic);
      font.setBold(item->m_bold);
      return font;
    }
    else if(role < 64 && item->m_customData & 1ULL << role)
    {
      return item->data(index.column(), role);
    }

    return QVariant();
  }

private:
  RDTreeWidget *widget;
};

RDTreeWidgetItem::RDTreeWidgetItem(const std::initializer_list<QVariant> &values)
{
  m_text = values;
  m_icons.resize(m_text.size());
}

RDTreeWidgetItem::~RDTreeWidgetItem()
{
  if(m_parent)
    m_parent->removeChild(this);

  clear();

  delete m_data;
}

QVariant RDTreeWidgetItem::data(int column, int role) const
{
  if(column >= m_data->count())
    return QVariant();

  const QVector<RoleData> &dataVec = (*m_data)[column];

  for(const RoleData &d : dataVec)
  {
    if(d.role == role)
      return d.data;
  }

  return QVariant();
}

void RDTreeWidgetItem::setData(int column, int role, const QVariant &value)
{
  // we lazy allocate
  if(!m_data)
  {
    m_data = new QVector<QVector<RoleData>>;
    m_data->resize(qMax(m_text.count(), column + 1));
  }

  if(role < Qt::UserRole)
  {
    Q_ASSERT(role < 64);
    m_customData |= 1ULL << role;
  }

  // data is allowed to resize above the column count in the widget
  if(m_data->count() <= column)
    m_data->resize(column + 1);

  QVector<RoleData> &dataVec = (*m_data)[column];

  for(RoleData &d : dataVec)
  {
    if(d.role == role)
    {
      bool different = (d.data != value);

      d.data = value;

      if(different && role < Qt::UserRole)
        m_widget->m_model->itemChanged(this, {role});

      return;
    }
  }

  dataVec.push_back(RoleData(role, value));

  if(role < Qt::UserRole)
    m_widget->m_model->itemChanged(this, {role});
}

void RDTreeWidgetItem::addChild(RDTreeWidgetItem *item)
{
  int colCount = item->m_text.count();

  // remove it from any previous parent
  if(item->m_parent)
    item->m_parent->removeChild(this);

  // set up its new parent to us
  item->m_parent = this;

  // set the widget in case this changed
  item->setWidget(m_widget);

  // resize per-column vectors to column count
  item->m_text.resize(colCount);
  item->m_icons.resize(colCount);

  // data can resize up, but we don't resize it down.
  if(item->m_data)
    item->m_data->resize(qMax(item->m_data->count(), colCount));

  if(m_widget)
    m_widget->m_model->beginAddChild(this);

  // add to our list of children
  m_children.push_back(item);

  if(m_widget)
    m_widget->m_model->endAddChild(this);
}

void RDTreeWidgetItem::setWidget(RDTreeWidget *widget)
{
  if(widget == m_widget)
    return;

  // if the widget is different, we need to recurse to children
  m_widget = widget;
  for(RDTreeWidgetItem *item : m_children)
    item->setWidget(widget);
}

void RDTreeWidgetItem::dataChanged(int role)
{
  if(m_widget)
    m_widget->itemDataChanged(this, role);
}

RDTreeWidgetItem *RDTreeWidgetItem::takeChild(int index)
{
  if(m_widget)
    m_widget->m_model->beginRemoveChildren(this, index, index);

  m_children[index]->m_parent = NULL;
  RDTreeWidgetItem *ret = m_children.takeAt(index);

  if(m_widget)
    m_widget->m_model->endRemoveChildren();

  return ret;
}

void RDTreeWidgetItem::removeChild(RDTreeWidgetItem *child)
{
  if(m_widget)
  {
    int row = m_children.indexOf(child);
    m_widget->m_model->beginRemoveChildren(this, row, row);
  }

  m_children.removeOne(child);

  if(m_widget)
    m_widget->m_model->endRemoveChildren();
}

void RDTreeWidgetItem::clear()
{
  if(!childCount())
    return;

  if(m_widget)
    m_widget->m_model->beginRemoveChildren(this, 0, childCount() - 1);

  while(childCount() > 0)
  {
    RDTreeWidgetItem *child = takeChild(0);
    child->clear();
    delete child;
  }

  if(m_widget)
    m_widget->m_model->endRemoveChildren();
}
RDTreeWidget::RDTreeWidget(QWidget *parent) : QTreeView(parent)
{
  setMouseTracking(true);

  m_root = new RDTreeWidgetItem;
  m_root->m_widget = this;

  m_model = new RDTreeWidgetModel(this);
  QTreeView::setModel(m_model);

  QObject::connect(this, &RDTreeWidget::activated, [this](const QModelIndex &idx) {
    emit itemActivated(m_model->itemForIndex(idx), idx.column());
  });
  QObject::connect(this, &RDTreeWidget::clicked, [this](const QModelIndex &idx) {
    emit itemClicked(m_model->itemForIndex(idx), idx.column());
  });
  QObject::connect(this, &RDTreeWidget::doubleClicked, [this](const QModelIndex &idx) {
    emit itemDoubleClicked(m_model->itemForIndex(idx), idx.column());
  });
}

RDTreeWidget::~RDTreeWidget()
{
  delete m_root;
  delete m_model;
}

void RDTreeWidget::beginUpdate()
{
  m_queueUpdates = true;

  m_queuedItem = NULL;
  m_lowestIndex = m_highestIndex = qMakePair<int, int>(-1, -1);
  m_queuedRoles = 0;
}

void RDTreeWidget::endUpdate()
{
  m_queueUpdates = false;

  if(m_queuedRoles)
  {
    // if we updated multiple different trees we can't issue a single dataChanged for everything
    // under a parent. Refresh the whole model.
    if(m_queuedItem == NULL)
    {
      m_model->refresh();
    }
    else
    {
      QVector<int> roles;
      for(int r = 0; r < 64; r++)
      {
        if(m_queuedRoles & (1ULL << r))
          roles.push_back(r);
      }
      m_model->itemsChanged(m_queuedItem, m_lowestIndex, m_highestIndex, roles);
    }
  }
}

void RDTreeWidget::setColumns(const QStringList &columns)
{
  m_headers = columns;
  m_model->refresh();
}

void RDTreeWidget::setHeaderText(int column, const QString &text)
{
  m_headers[column] = text;
  m_model->headerRefresh();
}

RDTreeWidgetItem *RDTreeWidget::selectedItem() const
{
  QModelIndexList sel = selectionModel()->selectedIndexes();

  if(sel.isEmpty())
    return NULL;

  return m_model->itemForIndex(sel[0]);
}

RDTreeWidgetItem *RDTreeWidget::currentItem() const
{
  return m_model->itemForIndex(currentIndex());
}

void RDTreeWidget::setSelectedItem(RDTreeWidgetItem *node)
{
  selectionModel()->select(m_model->indexForItem(node, 0),
                           QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void RDTreeWidget::setCurrentItem(RDTreeWidgetItem *node)
{
  setCurrentIndex(m_model->indexForItem(node, 0));
}

RDTreeWidgetItem *RDTreeWidget::itemAt(const QPoint &p) const
{
  return m_model->itemForIndex(indexAt(p));
}

void RDTreeWidget::expandItem(RDTreeWidgetItem *item)
{
  expand(m_model->indexForItem(item, 0));
}

void RDTreeWidget::scrollToItem(RDTreeWidgetItem *node)
{
  scrollTo(m_model->indexForItem(node, 0));
}

void RDTreeWidget::clear()
{
  m_root->clear();
}

void RDTreeWidget::mouseMoveEvent(QMouseEvent *e)
{
  QModelIndex idx = indexAt(e->pos());
  RDTreeWidgetItem *item = m_model->itemForIndex(idx);

  if(idx.column() == m_hoverColumn && m_hoverHandCursor)
    setCursor(QCursor(Qt::PointingHandCursor));
  else
    unsetCursor();

  if(m_currentHoverItem == item || m_hoverColumn < 0)
    return;

  RDTreeWidgetItem *old = m_currentHoverItem;
  m_currentHoverItem = item;

  // it's only two items, don't try and make a range but just change them both
  QVector<int> roles = {Qt::DecorationRole, Qt::BackgroundRole, Qt::ForegroundRole};
  if(old)
    m_model->itemChanged(old, roles);
  m_model->itemChanged(item, roles);

  emit mouseMove(e);

  QTreeView::mouseMoveEvent(e);
}

void RDTreeWidget::mouseReleaseEvent(QMouseEvent *e)
{
  QModelIndex idx = indexAt(e->pos());

  if(idx.isValid() && idx.column() == m_hoverColumn && m_activateOnClick)
  {
    emit itemActivated(itemAt(e->pos()), idx.column());
  }

  QTreeView::mouseReleaseEvent(e);
}

void RDTreeWidget::leaveEvent(QEvent *e)
{
  unsetCursor();

  if(m_currentHoverItem)
  {
    RDTreeWidgetItem *item = m_currentHoverItem;
    m_currentHoverItem = NULL;
    m_model->itemChanged(item, {Qt::DecorationRole, Qt::BackgroundRole, Qt::ForegroundRole});
  }

  emit leave(e);

  QTreeView::leaveEvent(e);
}

void RDTreeWidget::focusOutEvent(QFocusEvent *event)
{
  if(m_clearSelectionOnFocusLoss)
    clearSelection();

  QTreeView::focusOutEvent(event);
}

void RDTreeWidget::keyPressEvent(QKeyEvent *e)
{
  emit(keyPress(e));
  QTreeView::keyPressEvent(e);
}

void RDTreeWidget::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
  emit itemSelectionChanged();

  QTreeView::selectionChanged(selected, deselected);
}

void RDTreeWidget::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
  emit currentItemChanged(m_model->itemForIndex(current), m_model->itemForIndex(previous));

  QTreeView::currentChanged(current, previous);
}

void RDTreeWidget::itemDataChanged(RDTreeWidgetItem *item, int role)
{
  if(m_queueUpdates)
  {
    m_queuedRoles |= (1ULL << role);

    // for now we only support updating the whole row, with all columns, even if only one column
    // changed.
    int row = item->m_parent->indexOfChild(item);

    // no queued updates yet, set up this one
    if(m_lowestIndex.first == -1)
    {
      m_queuedItem = item;
      m_lowestIndex = qMakePair<int, int>(row, 0);
      m_highestIndex = qMakePair<int, int>(m_lowestIndex.first, m_headers.count() - 1);
    }
    else
    {
      // there's already an update. Check if we can expand it
      if(m_queuedItem == item)
      {
        m_lowestIndex.first = qMin(m_lowestIndex.first, row);
        m_highestIndex.first = qMax(m_highestIndex.first, row);
      }
      else
      {
        // can't batch updates across multiple parents, so we just fallback to full model refresh
        m_queuedItem = NULL;
      }
    }
  }
  else
  {
    m_model->itemChanged(item, {role});
  }
}
