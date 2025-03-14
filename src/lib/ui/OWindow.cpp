#include "OWindow.h"

#include <QtSvg/QSvgRenderer>

#include <QPainter>

namespace lib::ui {

OWindow::OWindow(const QString& icon, QWidget* parent) : QMainWindow{parent}, icon{icon} {
    updateIcon();
}

void OWindow::updateIcon()
{
    if(!icon.isEmpty()){

        // const QString color = "light";
        // const QString assetSuffix = "online";
        // QString path = ":/img/taskbar/" + color + "/taskbar_" + assetSuffix + ".svg";

        QSvgRenderer renderer(icon);
        // Prepare a QImage with desired characteritisc
        QImage image = QImage(250, 250, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        renderer.render(&painter);
        auto ico = QIcon(QPixmap::fromImage(image));
        setWindowIcon(ico);
    }
}
}
