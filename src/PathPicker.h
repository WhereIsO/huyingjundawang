// PathPicker.h — "标签 + 输入框 + 浏览按钮" 组合控件，便于复用
#pragma once
#include <QWidget>
class QLineEdit;
class QPushButton;
class QLabel;

namespace pbackup::ui {

class PathPicker : public QWidget {
    Q_OBJECT
public:
    enum Kind { Directory, OpenFile, SaveFile };
    explicit PathPicker(const QString& label, Kind kind, QWidget* parent = nullptr);

    QString text() const;
    void setText(const QString& path);
    void setPlaceholder(const QString& hint);

signals:
    void textChanged(const QString& text);

private slots:
    void onBrowse();
    void onTextEdited(const QString& t);

private:
    QLabel*      m_label = nullptr;
    QLineEdit*   m_edit  = nullptr;
    QPushButton* m_btn   = nullptr;
    Kind         m_kind;
};

} // namespace pbackup::ui
