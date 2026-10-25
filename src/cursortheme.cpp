#include "cursortheme.h"

#include "palette.h"
#include "paths.h"

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <QVector>
#include <QtEndian>

namespace {

enum class Shape {
    Arrow,
    Hand,
    Text,
    Crosshair,
    Move,
    Horizontal,
    Vertical,
    DiagonalDown,
    DiagonalUp,
    Watch,
    Progress,
    NotAllowed,
    Help,
    Menu,
    ZoomIn,
    ZoomOut,
};

struct CursorSpec {
    Shape shape;
    const char *name;
};

const QVector<CursorSpec> &specs()
{
    static const QVector<CursorSpec> list = {
        {Shape::Arrow, "left_ptr"},
        {Shape::Arrow, "default"},
        {Shape::Arrow, "arrow"},
        {Shape::Arrow, "top_left_arrow"},
        {Shape::Hand, "hand1"},
        {Shape::Hand, "hand2"},
        {Shape::Hand, "pointer"},
        {Shape::Hand, "grab"},
        {Shape::Hand, "grabbing"},
        {Shape::Hand, "openhand"},
        {Shape::Hand, "closedhand"},
        {Shape::Text, "text"},
        {Shape::Text, "xterm"},
        {Shape::Text, "vertical-text"},
        {Shape::Crosshair, "cross"},
        {Shape::Crosshair, "crosshair"},
        {Shape::Crosshair, "cell"},
        {Shape::Crosshair, "tcross"},
        {Shape::Crosshair, "X_cursor"},
        {Shape::Crosshair, "plus"},
        {Shape::Move, "fleur"},
        {Shape::Move, "all-scroll"},
        {Shape::Move, "move"},
        {Shape::Move, "all-resize"},
        {Shape::Horizontal, "sb_h_double_arrow"},
        {Shape::Horizontal, "ew-resize"},
        {Shape::Horizontal, "e-resize"},
        {Shape::Horizontal, "w-resize"},
        {Shape::Horizontal, "left_side"},
        {Shape::Horizontal, "right_side"},
        {Shape::Horizontal, "col-resize"},
        {Shape::Horizontal, "split_h"},
        {Shape::Vertical, "sb_v_double_arrow"},
        {Shape::Vertical, "ns-resize"},
        {Shape::Vertical, "n-resize"},
        {Shape::Vertical, "s-resize"},
        {Shape::Vertical, "top_side"},
        {Shape::Vertical, "bottom_side"},
        {Shape::Vertical, "row-resize"},
        {Shape::Vertical, "split_v"},
        {Shape::DiagonalDown, "top_left_corner"},
        {Shape::DiagonalDown, "bottom_right_corner"},
        {Shape::DiagonalDown, "nwse-resize"},
        {Shape::DiagonalDown, "nw-resize"},
        {Shape::DiagonalDown, "se-resize"},
        {Shape::DiagonalDown, "size_fdiag"},
        {Shape::DiagonalUp, "top_right_corner"},
        {Shape::DiagonalUp, "bottom_left_corner"},
        {Shape::DiagonalUp, "nesw-resize"},
        {Shape::DiagonalUp, "ne-resize"},
        {Shape::DiagonalUp, "sw-resize"},
        {Shape::DiagonalUp, "size_bdiag"},
        {Shape::Watch, "watch"},
        {Shape::Watch, "wait"},
        {Shape::Progress, "progress"},
        {Shape::Progress, "left_ptr_watch"},
        {Shape::NotAllowed, "not-allowed"},
        {Shape::NotAllowed, "no-drop"},
        {Shape::NotAllowed, "crossed_circle"},
        {Shape::Help, "question_arrow"},
        {Shape::Help, "help"},
        {Shape::Help, "whats_this"},
        {Shape::Menu, "context-menu"},
        {Shape::ZoomIn, "zoom-in"},
        {Shape::ZoomOut, "zoom-out"},
    };
    return list;
}

struct CursorColors {
    QColor accent;
    QColor contrast;
    QColor error;
    QColor errorContrast;
};

QPointF hotspot(Shape shape)
{
    switch (shape) {
    case Shape::Arrow:
        return QPointF(3.0, 2.0);
    case Shape::Hand:
        return QPointF(11.1, 2.8);
    case Shape::Text:
    case Shape::Crosshair:
    case Shape::Move:
    case Shape::Horizontal:
    case Shape::Vertical:
    case Shape::DiagonalDown:
    case Shape::DiagonalUp:
    case Shape::Watch:
    case Shape::NotAllowed:
        return QPointF(12.0, 12.0);
    case Shape::Progress:
    case Shape::Help:
        return QPointF(3.0, 2.0);
    case Shape::Menu:
        return QPointF(4.0, 5.0);
    case Shape::ZoomIn:
    case Shape::ZoomOut:
        return QPointF(10.5, 10.5);
    }
    return QPointF(12.0, 12.0);
}

QPainterPath triangle(const QPointF &a, const QPointF &b, const QPointF &c)
{
    QPainterPath path(a);
    path.lineTo(b);
    path.lineTo(c);
    path.closeSubpath();
    return path;
}

QPainterPath arrowPath()
{
    QPainterPath path(QPointF(3.0, 1.6));
    path.lineTo(3.0, 19.0);
    path.lineTo(7.9, 14.3);
    path.lineTo(11.0, 21.4);
    path.lineTo(14.0, 20.1);
    path.lineTo(10.9, 13.2);
    path.lineTo(17.2, 13.2);
    path.closeSubpath();
    return path;
}

QPainterPath handPath()
{
    QPainterPath palm;
    palm.addRoundedRect(QRectF(6.2, 10.6, 12.4, 9.8), 4.4, 4.4);

    QPainterPath finger;
    finger.addRoundedRect(QRectF(9.2, 2.6, 3.8, 11.6), 1.9, 1.9);

    QPainterPath thumbBase;
    thumbBase.addRoundedRect(QRectF(-1.4, 0.0, 2.8, 8.6), 1.4, 1.4);
    const QPainterPath thumb = QTransform().translate(8.5, 11.2).rotate(38.0).map(thumbBase);

    return finger.united(palm).united(thumb);
}

QPainterPath textPath()
{
    QPainterPath path;
    path.addRoundedRect(QRectF(10.8, 4.0, 2.4, 16.0), 1.2, 1.2);
    path.addRoundedRect(QRectF(7.4, 3.2, 9.2, 2.2), 1.1, 1.1);
    path.addRoundedRect(QRectF(7.4, 18.6, 9.2, 2.2), 1.1, 1.1);
    return path;
}

QPainterPath movePath()
{
    QPainterPath path;
    path.addRect(QRectF(10.7, 4.0, 2.6, 16.0));
    path.addRect(QRectF(4.0, 10.7, 16.0, 2.6));
    path = path.united(triangle(QPointF(12.0, 1.2), QPointF(8.6, 6.0), QPointF(15.4, 6.0)));
    path = path.united(triangle(QPointF(12.0, 22.8), QPointF(8.6, 18.0), QPointF(15.4, 18.0)));
    path = path.united(triangle(QPointF(1.2, 12.0), QPointF(6.0, 8.6), QPointF(6.0, 15.4)));
    path = path.united(triangle(QPointF(22.8, 12.0), QPointF(18.0, 8.6), QPointF(18.0, 15.4)));
    return path;
}

QPainterPath horizontalPath()
{
    QPainterPath path;
    path.addRect(QRectF(4.4, 10.8, 15.2, 2.4));
    path = path.united(triangle(QPointF(1.2, 12.0), QPointF(6.0, 8.6), QPointF(6.0, 15.4)));
    path = path.united(triangle(QPointF(22.8, 12.0), QPointF(18.0, 8.6), QPointF(18.0, 15.4)));
    return path;
}

QPainterPath rotatePath(QPainterPath path, qreal angle, qreal scale)
{
    QTransform transform;
    transform.translate(12.0, 12.0);
    transform.rotate(angle);
    transform.scale(scale, scale);
    transform.translate(-12.0, -12.0);
    return transform.map(path);
}

QPainterPath menuPath()
{
    QPainterPath path;
    path.addRoundedRect(QRectF(3.0, 5.0, 18.0, 14.0), 2.4, 2.4);
    return path;
}

void paintFilled(QPainter &painter, const QPainterPath &path, const QColor &fill,
                 const QColor &outline, qreal width = 1.3)
{
    painter.setPen(QPen(outline, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(fill);
    painter.drawPath(path);
}

void paintStrokes(QPainter &painter, const QVector<QLineF> &lines, const QColor &colour,
                  const QColor &outline, qreal width)
{
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(outline, width + 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const QLineF &line : lines)
        painter.drawLine(line);
    painter.setPen(QPen(colour, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const QLineF &line : lines)
        painter.drawLine(line);
}

void paintWatch(QPainter &painter, const QPointF &centre, qreal radius, const CursorColors &colors)
{
    painter.setPen(QPen(colors.contrast, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(colors.accent);
    painter.drawEllipse(centre, radius, radius);

    painter.setPen(QPen(colors.contrast, 1.5, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(centre, centre + QPointF(0.0, -radius * 0.62));
    painter.drawLine(centre, centre + QPointF(radius * 0.42, radius * 0.28));
    painter.setBrush(colors.contrast);
    painter.drawEllipse(centre, 0.7, 0.7);
}

void paintShape(QPainter &painter, Shape shape, const CursorColors &colors)
{
    switch (shape) {
    case Shape::Arrow:
        paintFilled(painter, arrowPath(), colors.accent, colors.contrast);
        break;
    case Shape::Hand:
        paintFilled(painter, handPath(), colors.accent, colors.contrast);
        break;
    case Shape::Text:
        paintFilled(painter, textPath(), colors.accent, colors.contrast);
        break;
    case Shape::Crosshair:
        paintStrokes(painter,
                     {QLineF(12.0, 1.8, 12.0, 8.6), QLineF(12.0, 15.4, 12.0, 22.2),
                      QLineF(1.8, 12.0, 8.6, 12.0), QLineF(15.4, 12.0, 22.2, 12.0)},
                     colors.accent, colors.contrast, 1.7);
        break;
    case Shape::Move:
        paintFilled(painter, movePath(), colors.accent, colors.contrast);
        break;
    case Shape::Horizontal:
        paintFilled(painter, horizontalPath(), colors.accent, colors.contrast);
        break;
    case Shape::Vertical:
        paintFilled(painter, rotatePath(horizontalPath(), 90.0, 1.0), colors.accent, colors.contrast);
        break;
    case Shape::DiagonalDown:
        paintFilled(painter, rotatePath(horizontalPath(), 45.0, 0.9), colors.accent, colors.contrast);
        break;
    case Shape::DiagonalUp:
        paintFilled(painter, rotatePath(horizontalPath(), -45.0, 0.9), colors.accent, colors.contrast);
        break;
    case Shape::Watch: {
        painter.setPen(QPen(colors.contrast, 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(colors.accent);
        painter.drawRoundedRect(QRectF(10.4, 1.9, 3.2, 2.8), 0.8, 0.8);
        paintWatch(painter, QPointF(12.0, 13.0), 8.4, colors);
        break;
    }
    case Shape::Progress: {
        QTransform transform;
        transform.scale(0.68, 0.68);
        transform.translate(0.6, 0.6);
        paintFilled(painter, transform.map(arrowPath()), colors.accent, colors.contrast, 1.5);
        paintWatch(painter, QPointF(18.0, 17.2), 4.0, colors);
        break;
    }
    case Shape::NotAllowed: {
        painter.setPen(QPen(colors.errorContrast, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(colors.error);
        painter.drawEllipse(QRectF(3.2, 3.2, 17.6, 17.6));
        painter.setPen(QPen(colors.errorContrast, 2.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(7.4, 7.4), QPointF(16.6, 16.6));
        break;
    }
    case Shape::Help: {
        QTransform transform;
        transform.scale(0.7, 0.7);
        transform.translate(0.4, 0.4);
        paintFilled(painter, transform.map(arrowPath()), colors.accent, colors.contrast, 1.5);

        QPainterPath question(QPointF(16.6, 13.8));
        question.cubicTo(16.6, 12.6, 17.4, 11.9, 18.4, 11.6);
        question.cubicTo(19.6, 11.2, 20.2, 10.4, 20.2, 9.5);
        question.cubicTo(20.2, 8.3, 19.4, 7.5, 18.3, 7.5);
        question.cubicTo(17.2, 7.5, 16.5, 8.2, 16.5, 9.2);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(colors.contrast, 4.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(question);
        painter.setPen(QPen(colors.accent, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(question);

        painter.setPen(QPen(colors.contrast, 0.8));
        painter.setBrush(colors.accent);
        painter.drawEllipse(QRectF(17.6, 14.9, 1.8, 1.8));
        break;
    }
    case Shape::Menu:
        paintFilled(painter, menuPath(), colors.accent, colors.contrast);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(colors.contrast, 1.4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(6.4, 9.4), QPointF(17.6, 9.4));
        painter.drawLine(QPointF(6.4, 12.6), QPointF(17.6, 12.6));
        painter.drawLine(QPointF(6.4, 15.8), QPointF(17.6, 15.8));
        break;
    case Shape::ZoomIn:
    case Shape::ZoomOut: {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(colors.contrast, 4.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(QRectF(3.4, 3.4, 13.2, 13.2));
        painter.setPen(QPen(colors.accent, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(QRectF(3.4, 3.4, 13.2, 13.2));

        painter.setPen(QPen(colors.contrast, 4.6, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(15.4, 15.4), QPointF(21.0, 21.0));
        painter.setPen(QPen(colors.accent, 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(15.4, 15.4), QPointF(21.0, 21.0));

        const QVector<QLineF> sign = {
            QLineF(7.0, 10.0, 13.0, 10.0),
            QLineF(10.0, 7.0, 10.0, 13.0),
        };
        painter.setPen(QPen(colors.contrast, 2.8, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(sign.first());
        if (shape == Shape::ZoomIn)
            painter.drawLine(sign.last());
        painter.setPen(QPen(colors.accent, 1.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(sign.first());
        if (shape == Shape::ZoomIn)
            painter.drawLine(sign.last());
        break;
    }
    }
}

QImage render(Shape shape, int size, const CursorColors &colors)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(size / 24.0, size / 24.0);
    paintShape(painter, shape, colors);
    painter.end();

    return image;
}

void put32(QByteArray &data, quint32 value)
{
    char bytes[4];
    qToLittleEndian(value, bytes);
    data.append(bytes, 4);
}

QByteArray encodeXcursor(const QVector<QImage> &images, const QPointF &spot)
{
    QByteArray data;
    data.reserve(16 + images.size() * 12 + images.size() * 4096);

    put32(data, 0x72756358); // "Xcur"
    put32(data, 16);
    put32(data, 0x00010000);
    put32(data, static_cast<quint32>(images.size()));

    quint32 position = 16 + images.size() * 12;
    for (const QImage &image : images) {
        put32(data, 0xfffd0002);
        put32(data, static_cast<quint32>(image.width()));
        put32(data, position);
        position += 36 + image.width() * image.height() * 4;
    }

    for (const QImage &image : images) {
        const int size = image.width();
        const int xhot = qBound(0, qRound(spot.x() * size / 24.0), size - 1);
        const int yhot = qBound(0, qRound(spot.y() * size / 24.0), size - 1);

        put32(data, 36);
        put32(data, 0xfffd0002);
        put32(data, static_cast<quint32>(size));
        put32(data, 1);
        put32(data, static_cast<quint32>(size));
        put32(data, static_cast<quint32>(size));
        put32(data, static_cast<quint32>(xhot));
        put32(data, static_cast<quint32>(yhot));
        put32(data, 0);

        for (int y = 0; y < image.height(); ++y) {
            const auto *line = reinterpret_cast<const quint32 *>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                char pixel[4];
                qToLittleEndian(line[x], pixel);
                data.append(pixel, 4);
            }
        }
    }

    return data;
}

QString clean(QString text)
{
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
    return text.trimmed();
}

} // namespace

QString CursorTheme::themeName()
{
    return QStringLiteral("hyperthemes");
}

QString CursorTheme::installDir()
{
    return Paths::dataHome() + QStringLiteral("/icons/") + themeName();
}

QString CursorTheme::defaultThemePath()
{
    return Paths::home() + QStringLiteral("/.icons/default/index.theme");
}

QByteArray CursorTheme::defaultThemeFile()
{
    return QStringLiteral(
               "[Icon Theme]\n"
               "Name=HyperThemes\n"
               "Comment=Use the HyperThemes pointer in every application\n"
               "Inherits=%1\n")
        .arg(themeName())
        .toUtf8();
}

QHash<QString, QByteArray> CursorTheme::build(const Theme &theme)
{
    const QVariantMap palette = Palette::normalise(theme.palette);
    const QColor accent = Palette::colour(palette, QStringLiteral("primary"), QColor(QStringLiteral("#8ab4f8")));
    const QColor error = Palette::colour(palette, QStringLiteral("error"), QColor(QStringLiteral("#ff6b6b")));

    const CursorColors colors{
        accent,
        QColor(Palette::withHash(Palette::onColour(accent))),
        error,
        QColor(Palette::withHash(Palette::onColour(error))),
    };

    QHash<int, QByteArray> encoded;
    QHash<QString, QByteArray> files;
    for (const CursorSpec &spec : specs()) {
        const int key = static_cast<int>(spec.shape);
        if (!encoded.contains(key)) {
            QVector<QImage> images;
            for (const int size : {24, 32, 48})
                images.append(render(spec.shape, size, colors));
            encoded.insert(key, encodeXcursor(images, hotspot(spec.shape)));
        }
        files.insert(QStringLiteral("cursors/") + QString::fromLatin1(spec.name), encoded.value(key));
    }

    const QString name = clean(theme.name).isEmpty() ? theme.id : clean(theme.name);
    const QByteArray index = QStringLiteral(
                                 "[Icon Theme]\n"
                                 "Name=HyperThemes\n"
                                 "Comment=Pointer generated from %1\n"
                                 "Inherits=Adwaita\n")
                                 .arg(name)
                                 .toUtf8();
    files.insert(QStringLiteral("index.theme"), index);

    return files;
}
