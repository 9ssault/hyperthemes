#pragma once

#include <QImage>
#include <QQuickPaintedItem>
#include <QVariantMap>

/// Draws one generated pointer shape in the inspector. Rendering is cached, so
/// a grid of these stays cheap while the size slider moves.
class PointerShapeItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString shape READ shape WRITE setShape NOTIFY shapeChanged)
    Q_PROPERTY(QVariantMap themePalette READ themePalette WRITE setThemePalette NOTIFY themePaletteChanged)
    Q_PROPERTY(int size READ size WRITE setSize NOTIFY sizeChanged)

public:
    explicit PointerShapeItem(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    QString shape() const { return m_shape; }
    void setShape(const QString &shape);

    QVariantMap themePalette() const { return m_palette; }
    void setThemePalette(const QVariantMap &palette);

    int size() const { return m_size; }
    void setSize(int size);

signals:
    void shapeChanged();
    void themePaletteChanged();
    void sizeChanged();

private:
    void invalidate();

    QString m_shape;
    QVariantMap m_palette;
    int m_size = 32;
    QImage m_image;
};
