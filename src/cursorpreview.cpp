#include "cursorpreview.h"

#include "cursortheme.h"

#include <QPainter>

PointerShapeItem::PointerShapeItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setOpaquePainting(false);
    setAntialiasing(true);
}

void PointerShapeItem::setShape(const QString &shape)
{
    if (m_shape == shape)
        return;
    m_shape = shape;
    invalidate();
    emit shapeChanged();
}

void PointerShapeItem::setThemePalette(const QVariantMap &palette)
{
    if (m_palette == palette)
        return;
    m_palette = palette;
    invalidate();
    emit themePaletteChanged();
}

void PointerShapeItem::setSize(int size)
{
    const int clamped = CursorTheme::clampSize(size);
    if (m_size == clamped)
        return;
    m_size = clamped;
    invalidate();
    emit sizeChanged();
}

void PointerShapeItem::invalidate()
{
    m_image = QImage();
    update();
}

void PointerShapeItem::paint(QPainter *painter)
{
    if (contentsBoundingRect().isEmpty())
        return;

    if (m_image.isNull() && !m_shape.isEmpty())
        m_image = CursorTheme::render(m_palette, m_shape, m_size);
    if (m_image.isNull())
        return;

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawImage(contentsBoundingRect(), m_image);
}
