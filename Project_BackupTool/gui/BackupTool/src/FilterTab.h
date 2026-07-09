// FilterTab.h — 筛选页：路径 / 类型 / 名称 / 时间 / 大小 / 用户 六类过滤
#pragma once
#include <QWidget>
#include "BackendAdapter.h"

class QLineEdit;
class QCheckBox;
class QDateEdit;

namespace pbackup::ui {

class FilterTab : public QWidget {
    Q_OBJECT
public:
    explicit FilterTab(QWidget* parent = nullptr);

    FilterSpec buildSpec() const;   // 从界面收集 6 类筛选条件

public slots:
    void setBusy(bool busy);

private:
    // 路径
    QLineEdit* m_includePath = nullptr;
    QLineEdit* m_excludePath = nullptr;
    // 名称
    QLineEdit* m_nameGlob    = nullptr;
    // 类型（复选）
    QCheckBox* m_useTypeFilter = nullptr;
    QCheckBox* m_typeFile     = nullptr;
    QCheckBox* m_typeDir      = nullptr;
    QCheckBox* m_typeEmptyDir = nullptr;
    QCheckBox* m_typeSymlink  = nullptr;
    QCheckBox* m_typeHardlink = nullptr;
    QCheckBox* m_typeJunction = nullptr;
    QCheckBox* m_typeReparse  = nullptr;
    // 大小
    QLineEdit* m_sizeMin     = nullptr;
    QLineEdit* m_sizeMax     = nullptr;
    // 时间
    QCheckBox* m_useMtime    = nullptr;
    QDateEdit* m_mtimeAfter  = nullptr;
    QDateEdit* m_mtimeBefore = nullptr;
    // 用户
    QLineEdit* m_ownerSid    = nullptr;
};

} // namespace pbackup::ui
