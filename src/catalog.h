#pragma once

#include <QObject>
#include <QVariantList>

/// Loads the bundled theme catalog. Every theme lives in its own JSON file
/// below :/catalog/<category>/, so the shelf can grow one file at a time.
class Catalog : public QObject {
    Q_OBJECT

public:
    explicit Catalog(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList load() const;
};
