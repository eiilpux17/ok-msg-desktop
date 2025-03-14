#ifndef OWINDOW_H
#define OWINDOW_H

#include <QMainWindow>
#include <QObject>

namespace lib::ui {
class OWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit OWindow(const QString& icon = {}, QWidget* parent = nullptr);

    void setIcon(const QString& icon);
private:
    void updateIcon();

    QString icon;
signals:
};
}
#endif  // OWINDOW_H
