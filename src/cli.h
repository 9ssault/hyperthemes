#pragma once

#include <QString>
#include <QStringList>

namespace Cli {

struct Result {
    bool handled = false;
    int exitCode = 0;
    QString output;
    QString error;
};

/// True when the arguments contain one of the headless options. The GUI stays
/// the default, so unknown Qt or desktop arguments never switch modes.
bool requested(const QStringList &arguments);

/// Runs the headless command, if one was requested. The caller owns printing
/// and the process exit code; no window is created.
Result run(const QStringList &arguments);

QString helpText();

} // namespace Cli
