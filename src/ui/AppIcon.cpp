#include "AppIcon.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {

QPixmap renderIcon(int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal scale = size / 128.0;
    painter.scale(scale, scale);

    QLinearGradient background(10, 8, 118, 120);
    background.setColorAt(0.0, QColor("#5B86F7"));
    background.setColorAt(1.0, QColor("#2857D6"));
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(QRectF(5, 5, 118, 118), 27, 27);

    painter.setBrush(QColor(255, 255, 255, 245));
    painter.drawRoundedRect(QRectF(28, 20, 72, 88), 11, 11);

    QPen linePen(QColor("#B8C8EA"), 5, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(linePen);
    painter.drawLine(QPointF(43, 44), QPointF(84, 44));
    painter.drawLine(QPointF(43, 59), QPointF(78, 59));
    painter.drawLine(QPointF(43, 74), QPointF(68, 74));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#23B985"));
    painter.drawEllipse(QPointF(91, 91), 25, 25);

    QPainterPath check;
    check.moveTo(79, 91);
    check.lineTo(88, 100);
    check.lineTo(104, 82);
    painter.setPen(QPen(Qt::white, 6, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(check);

    return pixmap;
}

} // namespace

QIcon AppIcon::create()
{
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64, 128, 256})
        icon.addPixmap(renderIcon(size));
    return icon;
}
