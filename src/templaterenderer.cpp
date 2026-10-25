#include "templaterenderer.h"

#include "palette.h"
#include "paths.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

QString TemplateRenderer::render(const QString &templateName, const Theme &theme)
{
    QFile file(Paths::templatePath(templateName));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return renderText(QString::fromUtf8(file.readAll()), theme);
}

QString TemplateRenderer::renderText(const QString &text, const Theme &theme)
{
    const QVariantMap palette = Palette::normalise(theme.palette);
    QString output = text;

    output.replace(QStringLiteral("{{name}}"), theme.name);
    output.replace(QStringLiteral("{{mode}}"), theme.mode);
    output.replace(QStringLiteral("{{variant}}"), theme.variant);
    output.replace(QStringLiteral("{{flavour}}"), theme.flavour);
    output.replace(QStringLiteral("{{source}}"), theme.source);

    static const QRegularExpression expression(QStringLiteral("\\{\\{\\s*([A-Za-z0-9_]+)(?:\\.([A-Za-z0-9_]+))?\\s*\\}\\}"));
    for (int guard = 0; guard < 4096; ++guard) {
        const QRegularExpressionMatch match = expression.match(output);
        if (!match.hasMatch())
            break;

        const QString placeholder = match.captured(0);
        const QString role = match.captured(1);
        const QString form = match.captured(2);
        QString replacement;
        if (form == QLatin1String("rgb")) {
            replacement = Palette::rgb(palette, role);
        } else if (form == QLatin1String("rgba")) {
            const QColor c = Palette::colour(palette, role);
            replacement = QStringLiteral("%1,%2,%3,1").arg(c.red()).arg(c.green()).arg(c.blue());
        } else if (form == QLatin1String("stripped")) {
            replacement = Palette::hex(palette, role);
        } else if (form.isEmpty()) {
            replacement = Palette::hex(palette, role);
        } else {
            replacement = Palette::hex(palette, form);
        }

        output.replace(placeholder, replacement);
    }

    return output;
}
