#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QString>

#include "audio/SoundEngine.h"
#include "i18n/I18n.h"
#include "ui/MainWindow.h"
#include "ui/ThemeManager.h"

namespace {

// Per-user, per-app local socket name.  Including the user name keeps
// the lock isolated when several users run the app concurrently on the
// same machine, while the constant suffix prevents collisions with
// unrelated apps.  We avoid env-var dependent names in the binary so
// the same name can be re-derived inside a child process or a launcher.
QString singleInstanceServerName()
{
    return QStringLiteral("VolchayCleans.singleInstance.v1");
}

// Try to hand off to an already-running instance: connect to the named
// local server and write a "raise" payload.  Returns true if the
// hand-off succeeded — in which case this process should exit silently
// without launching a second window.
bool handoffToRunningInstance()
{
    QLocalSocket socket;
    socket.connectToServer(singleInstanceServerName());
    if (!socket.waitForConnected(300)) {
        return false;
    }
    socket.write("raise");
    socket.flush();
    socket.waitForBytesWritten(300);
    socket.disconnectFromServer();
    return true;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication::setApplicationName(QStringLiteral("Volchay Cleans"));
    QApplication::setApplicationDisplayName(QStringLiteral("Volchay Cleans"));
    QApplication::setApplicationVersion(QStringLiteral("0.2.0"));
    QApplication::setOrganizationName(QStringLiteral("Volchay"));
    QApplication::setOrganizationDomain(QStringLiteral("volchay.local"));

    // QApplication must exist before any QLocalSocket / QLocalServer
    // operations — Qt's IPC primitives rely on the global event-loop
    // thread / posted-event machinery owned by the application object.
    QApplication app(argc, argv);

    // Single-instance lock.  If another copy of the app is already
    // running, ask it to bring its window to the foreground and exit.
    if (handoffToRunningInstance()) {
        return 0;
    }
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/logo.svg")));

    // Try to load bundled font, but fall back gracefully.
    const int fontId = QFontDatabase::addApplicationFont(
        QStringLiteral(":/fonts/Inter-Regular.ttf"));
    if (fontId >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) {
            QFont f = app.font();
            f.setFamily(families.first());
            app.setFont(f);
        }
    }

    I18n::instance().loadFromSettings();
    SoundEngine::instance().loadFromSettings();
    ThemeManager::instance().applySaved(&app);

    MainWindow w;
    w.show();

    // Listen for "raise" messages from secondary instances and bring
    // the window forward.  We remove any stale socket left by a
    // previously-crashed instance before listening.
    QLocalServer server;
    QLocalServer::removeServer(singleInstanceServerName());
    if (server.listen(singleInstanceServerName())) {
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&] {
            while (auto* client = server.nextPendingConnection()) {
                QObject::connect(client, &QLocalSocket::disconnected,
                                 client, &QLocalSocket::deleteLater);
                w.activateFromBackground();
            }
        });
    }

    return app.exec();
}
