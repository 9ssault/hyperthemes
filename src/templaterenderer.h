#pragma once

#include "theme.h"

#include <QString>

class TemplateRenderer {
public:
    static QString render(const QString &templateName, const Theme &theme);
    static QString renderText(const QString &text, const Theme &theme);
};
