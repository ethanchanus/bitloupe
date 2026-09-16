// SPDX-FileCopyrightText: 2007-2010, 2013-2014, 2016, 2026 BitLoupe developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/mainwindow.h"
#include "core/settings.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QApplication>
#include <QColorSpace>
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QAbstractSocket>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMetaObject>
#include <QKeySequence>
#include <QSurfaceFormat>
#include <QEvent>
#include <QTextStream>
#include <QThread>
#include <QVariant>

#include <atomic>
#include <cstdio>
#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#ifdef BITLOUPE_SOCREGS_DIAGNOSTICS
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")
#endif
#endif

#ifdef Q_OS_UNIX
#include <QSocketNotifier>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#ifdef Q_OS_WIN
QString focusHotkeyConfigurationPath()
{
    const QString overridePath = qEnvironmentVariable("BITLOUPE_SHORTCUT_CONFIG").trimmed();
    if (!overridePath.isEmpty())
        return QFileInfo(overridePath).absoluteFilePath();

    const QDir applicationDirectory(QCoreApplication::applicationDirPath());
    const QString confPath = applicationDirectory.filePath(QStringLiteral("conf/settings.conf"));
    if (QFileInfo::exists(confPath))
        return QFileInfo(confPath).absoluteFilePath();

    const QString applicationPath = applicationDirectory.filePath(QStringLiteral("settings.conf"));
    if (QFileInfo::exists(applicationPath))
        return applicationPath;

    const QString parentPath = applicationDirectory.filePath(QStringLiteral("../settings.conf"));
    if (QFileInfo::exists(parentPath))
        return QFileInfo(parentPath).absoluteFilePath();

    return confPath;
}

QString configuredFocusHotkey()
{
    QFile file(focusHotkeyConfigurationPath());
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("Ctrl+Shift+`");

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return QStringLiteral("Ctrl+Shift+`");

    const QString configured = document.object().value(QStringLiteral("General.FocusHotkey")).toString().trimmed();
    return configured.isEmpty() ? QStringLiteral("Ctrl+Shift+`") : configured;
}

class GlobalFocusHotkey : public QAbstractNativeEventFilter {
public:
    explicit GlobalFocusHotkey(MainWindow* window)
        : m_window(window)
    {
        const QString configuredHotkey = configuredFocusHotkey();
        const QKeySequence sequence = QKeySequence::fromString(configuredHotkey,
                                                                 QKeySequence::PortableText);
        if (sequence.isEmpty())
            return;

        const int combinedKey = sequence[0].toCombined();
        const int key = combinedKey & ~Qt::KeyboardModifierMask;
        UINT modifiers = MOD_NOREPEAT;
        if (combinedKey & Qt::CTRL)
            modifiers |= MOD_CONTROL;
        if (combinedKey & Qt::SHIFT)
            modifiers |= MOD_SHIFT;
        if (combinedKey & Qt::ALT)
            modifiers |= MOD_ALT;
        if (combinedKey & Qt::META)
            modifiers |= MOD_WIN;

        UINT virtualKey = 0;
        if (key >= Qt::Key_A && key <= Qt::Key_Z)
            virtualKey = static_cast<UINT>('A' + key - Qt::Key_A);
        else if (key >= Qt::Key_0 && key <= Qt::Key_9)
            virtualKey = static_cast<UINT>('0' + key - Qt::Key_0);
        else if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
            virtualKey = static_cast<UINT>(VK_F1 + key - Qt::Key_F1);
        else if (key == Qt::Key_QuoteLeft)
            virtualKey = VK_OEM_3;

        if (virtualKey == 0)
            return;

        m_windowHandle = reinterpret_cast<HWND>(m_window->winId());
        m_registered = RegisterHotKey(m_windowHandle, m_id, modifiers, virtualKey) != FALSE;
    }

    ~GlobalFocusHotkey() override
    {
        if (m_registered)
            UnregisterHotKey(m_windowHandle, m_id);
    }

    bool nativeEventFilter(const QByteArray&, void* message, qintptr*) override
    {
        MSG* nativeMessage = static_cast<MSG*>(message);
        if (m_registered && nativeMessage != nullptr
            && nativeMessage->message == WM_HOTKEY
            && nativeMessage->wParam == static_cast<WPARAM>(m_id)) {
            m_window->activateAndFocusInput();
            return true;
        }
        return false;
    }

private:
    MainWindow* m_window = nullptr;
    HWND m_windowHandle = nullptr;
    const int m_id = 0x5343;
    bool m_registered = false;
};
#endif

namespace {

#ifdef BITLOUPE_SOCREGS_DIAGNOSTICS
// TEMPORARY (SoC Regs silicon-combo crash investigation): this is a WIN32-
// subsystem app, so qDebug/qWarning have no visible console even when
// launched from a terminal. Route them to a file instead so the reporter can
// reproduce and collect a log. Remove once the crash is diagnosed and fixed.
void socRegsFileMessageHandler(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    static QFile logFile(QDir::temp().filePath(QStringLiteral("bitloupe_socregs_debug.log")));
    static const bool opened = logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    if (!opened)
        return;
    const char* level = "DEBUG";
    switch (type) {
    case QtDebugMsg: level = "DEBUG"; break;
    case QtInfoMsg: level = "INFO"; break;
    case QtWarningMsg: level = "WARN"; break;
    case QtCriticalMsg: level = "CRIT"; break;
    case QtFatalMsg: level = "FATAL"; break;
    }
    QTextStream stream(&logFile);
    stream << QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"))
           << " [" << level << "] " << message << Qt::endl;
    stream.flush();
}

#ifdef Q_OS_WIN
// TEMPORARY (SoC Regs crash investigation): writes a symbolicated call stack
// to %TEMP%\bitloupe_crash.log plus a full minidump to
// %TEMP%\bitloupe_crash.dmp on any unhandled exception. Deliberately uses
// only plain Win32 file I/O (no Qt) since the process may already be in a
// corrupted state by the time this runs. Remove once the crash is fixed.
LONG WINAPI socRegsCrashHandler(EXCEPTION_POINTERS* exceptionInfo)
{
    wchar_t tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);

    wchar_t logPath[MAX_PATH];
    swprintf_s(logPath, L"%s%s", tempDir, L"bitloupe_crash.log");
    HANDLE file = CreateFileW(logPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        char line[1024];
        DWORD written = 0;
        int len = sprintf_s(line, "Unhandled exception 0x%08lX at address 0x%p\r\n",
                             exceptionInfo->ExceptionRecord->ExceptionCode,
                             exceptionInfo->ExceptionRecord->ExceptionAddress);
        WriteFile(file, line, static_cast<DWORD>(len), &written, nullptr);

        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        if (SymInitialize(process, nullptr, TRUE)) {
            CONTEXT context = *exceptionInfo->ContextRecord;
            STACKFRAME64 frame;
            memset(&frame, 0, sizeof(frame));
            DWORD machineType;
#if defined(_M_X64)
            machineType = IMAGE_FILE_MACHINE_AMD64;
            frame.AddrPC.Offset = context.Rip;
            frame.AddrPC.Mode = AddrModeFlat;
            frame.AddrFrame.Offset = context.Rbp;
            frame.AddrFrame.Mode = AddrModeFlat;
            frame.AddrStack.Offset = context.Rsp;
            frame.AddrStack.Mode = AddrModeFlat;
#else
            machineType = IMAGE_FILE_MACHINE_I386;
            frame.AddrPC.Offset = context.Eip;
            frame.AddrPC.Mode = AddrModeFlat;
            frame.AddrFrame.Offset = context.Ebp;
            frame.AddrFrame.Mode = AddrModeFlat;
            frame.AddrStack.Offset = context.Esp;
            frame.AddrStack.Mode = AddrModeFlat;
#endif
            for (int i = 0; i < 80; ++i) {
                if (!StackWalk64(machineType, process, thread, &frame, &context, nullptr,
                                  SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
                    break;
                if (frame.AddrPC.Offset == 0)
                    break;

                char symbolBuffer[sizeof(SYMBOL_INFO) + 256];
                memset(symbolBuffer, 0, sizeof(symbolBuffer));
                SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
                symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                symbol->MaxNameLen = 256;

                DWORD64 displacement = 0;
                DWORD lineDisplacement = 0;
                IMAGEHLP_LINE64 lineInfo;
                memset(&lineInfo, 0, sizeof(lineInfo));
                lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

                const bool haveSymbol = SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol);
                const bool haveLine = haveSymbol
                    && SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &lineInfo);

                if (haveLine) {
                    len = sprintf_s(line, "#%2d  0x%p  %s + 0x%llx  (%s:%lu)\r\n",
                                    i, reinterpret_cast<void*>(frame.AddrPC.Offset), symbol->Name,
                                    static_cast<unsigned long long>(displacement),
                                    lineInfo.FileName, lineInfo.LineNumber);
                } else if (haveSymbol) {
                    len = sprintf_s(line, "#%2d  0x%p  %s + 0x%llx\r\n",
                                    i, reinterpret_cast<void*>(frame.AddrPC.Offset), symbol->Name,
                                    static_cast<unsigned long long>(displacement));
                } else {
                    len = sprintf_s(line, "#%2d  0x%p  <unresolved symbol>\r\n",
                                    i, reinterpret_cast<void*>(frame.AddrPC.Offset));
                }
                WriteFile(file, line, static_cast<DWORD>(len), &written, nullptr);
            }
            SymCleanup(process);
        } else {
            const char* msg = "SymInitialize failed; no symbolicated stack available.\r\n";
            WriteFile(file, msg, static_cast<DWORD>(strlen(msg)), &written, nullptr);
        }
        CloseHandle(file);
    }

    wchar_t dumpPath[MAX_PATH];
    swprintf_s(dumpPath, L"%s%s", tempDir, L"bitloupe_crash.dmp");
    HANDLE dumpFile = CreateFileW(dumpPath, GENERIC_WRITE, 0, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dumpFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mdInfo;
        mdInfo.ThreadId = GetCurrentThreadId();
        mdInfo.ExceptionPointers = exceptionInfo;
        mdInfo.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile,
                          MiniDumpWithDataSegs, &mdInfo, nullptr, nullptr);
        CloseHandle(dumpFile);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif
#endif // BITLOUPE_SOCREGS_DIAGNOSTICS

// Dynamic property name mirrored by MainWindow. It lets the GUI layer observe
// shutdown state even when quit starts from the application/singleton layer.
constexpr const char* kShutdownInProgressProperty = "bitloupeShutdownInProgress";

QString singletonServerName()
{
    const QString scope = QCoreApplication::applicationFilePath()
        + QLatin1Char('|')
        + Settings::getConfigPath();
    const QByteArray hash = QCryptographicHash::hash(scope.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("bitloupe-single-instance-%1").arg(QString::fromLatin1(hash.left(24)));
}

void activateMainWindow(MainWindow* window)
{
    if (!window)
        return;

    if (window->windowState() & Qt::WindowMinimized)
        window->setWindowState(window->windowState() & ~Qt::WindowMinimized);

    window->show();
    window->raise();
    window->activateWindow();
}

bool notifyRunningInstance(const QString& serverName)
{
    QLocalSocket socket;
    socket.connectToServer(serverName, QIODevice::ReadWrite);
    if (!socket.waitForConnected(250))
        return false;

    socket.write("activate\n");
    if (!socket.waitForBytesWritten(250))
        return false;
    if (!socket.waitForReadyRead(750))
        return false;
    // Treat the instance as running only after it explicitly accepts the
    // activation. If it is already shutting down, the new process should take
    // over instead of exiting and leaving the user with no restored windows.
    const QByteArray response = socket.readAll().trimmed();
    socket.disconnectFromServer();
    return response == QByteArrayLiteral("accepted");
}

bool startSingletonServer(QLocalServer* server, const QString& serverName, bool* alreadyRunning)
{
    if (alreadyRunning)
        *alreadyRunning = false;

    if (server->listen(serverName))
        return true;

    if (server->serverError() == QAbstractSocket::AddressInUseError) {
        if (notifyRunningInstance(serverName)) {
            if (alreadyRunning)
                *alreadyRunning = true;
            return false;
        }
        QLocalServer::removeServer(serverName);
        if (server->listen(serverName))
            return true;
    }

    qWarning() << "Could not initialize singleton server:" << server->errorString();
    return false;
}

MainWindow* g_mainWindow = 0;
std::atomic_bool g_eventLoopRunning(false);
std::atomic_bool g_shutdownInProgress(false);
std::atomic_bool g_pendingActivation(false);

bool shutdownInProgress()
{
    const QCoreApplication* application = QCoreApplication::instance();
    // Keep both an atomic flag and the QApplication property: signal handlers
    // and singleton callbacks use the atomic, while MainWindow close handling
    // reads the property.
    return g_shutdownInProgress.load()
        || (application && application->property(kShutdownInProgressProperty).toBool());
}

class ShutdownEventFilter : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        Q_UNUSED(watched);
        if (event != nullptr && event->type() == QEvent::Quit) {
            // Mark shutdown as early as possible. aboutToQuit can be too late
            // for nested close events that decide whether to save child-window
            // layout changes.
            g_shutdownInProgress.store(true);
            if (QCoreApplication::instance())
                QCoreApplication::instance()->setProperty(kShutdownInProgressProperty, true);
        }
        return false;
    }
};

void persistAndQuit()
{
    // Console/signal shutdown paths bypass the normal window-close entry point,
    // so they must publish shutdown state before asking MainWindow to persist.
    g_shutdownInProgress.store(true);
    if (QCoreApplication::instance())
        QCoreApplication::instance()->setProperty(kShutdownInProgressProperty, true);
    if (g_mainWindow)
        g_mainWindow->persistSessionAndSettingsForShutdown();
    if (QCoreApplication::instance())
        QCoreApplication::quit();
}

void requestGracefulShutdown(Qt::ConnectionType connectionType = Qt::QueuedConnection)
{
    QCoreApplication* application = QCoreApplication::instance();
    if (!application)
        return;

    if (QThread::currentThread() == application->thread()) {
        persistAndQuit();
        return;
    }

    QMetaObject::invokeMethod(application, []() {
        persistAndQuit();
    }, connectionType);
}

#ifdef Q_OS_UNIX
int g_unixSignalFds[2] = { -1, -1 };

void handleUnixSignal(int signalNumber)
{
    const char signalCode = static_cast<char>(signalNumber);
    const ssize_t written = ::write(g_unixSignalFds[0], &signalCode, sizeof(signalCode));
    Q_UNUSED(written);
}

bool installUnixSignalHandler(int signalNumber)
{
    struct sigaction action = {};
    action.sa_handler = handleUnixSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    return ::sigaction(signalNumber, &action, 0) == 0;
}

bool setupUnixTerminationSignalHandlers()
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_unixSignalFds) != 0)
        return false;

    const bool ok = installUnixSignalHandler(SIGTERM)
        && installUnixSignalHandler(SIGINT)
        && installUnixSignalHandler(SIGHUP);
    if (!ok) {
        ::close(g_unixSignalFds[0]);
        ::close(g_unixSignalFds[1]);
        g_unixSignalFds[0] = -1;
        g_unixSignalFds[1] = -1;
    }

    return ok;
}
#endif

#ifdef Q_OS_WIN
BOOL WINAPI handleWindowsConsoleControl(DWORD controlType)
{
    switch (controlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        requestGracefulShutdown(g_eventLoopRunning.load()
            ? Qt::BlockingQueuedConnection
            : Qt::QueuedConnection);
        return TRUE;
    default:
        return FALSE;
    }
}
#endif

}

int main(int argc, char* argv[])
{
#ifdef BITLOUPE_SOCREGS_DIAGNOSTICS
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(socRegsCrashHandler);
#endif
    qInstallMessageHandler(socRegsFileMessageHandler);
#endif

    QSurfaceFormat surfaceFormat = QSurfaceFormat::defaultFormat();
    surfaceFormat.setColorSpace(QColorSpace::SRgb);
    QSurfaceFormat::setDefaultFormat(surfaceFormat);

    QApplication application(argc, argv);
#ifdef BITLOUPE_SOCREGS_DIAGNOSTICS
    qWarning() << "==== BitLoupe starting -- SoC Regs debug log active at"
               << QDir::temp().filePath(QStringLiteral("bitloupe_socregs_debug.log")) << "====";
#endif
    ShutdownEventFilter shutdownEventFilter(&application);
    application.installEventFilter(&shutdownEventFilter);

    QCoreApplication::setApplicationName("BitLoupe");
    QCoreApplication::setOrganizationDomain("bitloupe.org");
    QGuiApplication::setDesktopFileName("org.bitloupe.BitLoupe");

    Settings::instance();
    QLocalServer singletonServer;
    g_pendingActivation.store(false);

    QObject::connect(&application, &QCoreApplication::aboutToQuit, &application, [&]() {
        // Stop accepting single-instance activations during teardown. A second
        // launch that races with quit should create the replacement instance,
        // not send activation to a process that is already saving and exiting.
        g_shutdownInProgress.store(true);
        application.setProperty(kShutdownInProgressProperty, true);
        singletonServer.close();
        if (g_mainWindow)
            g_mainWindow->persistSessionAndSettingsForShutdown();
#ifdef Q_OS_UNIX
        if (g_unixSignalFds[0] != -1) {
            ::close(g_unixSignalFds[0]);
            g_unixSignalFds[0] = -1;
        }
        if (g_unixSignalFds[1] != -1) {
            ::close(g_unixSignalFds[1]);
            g_unixSignalFds[1] = -1;
        }
#endif
    });

#ifdef Q_OS_UNIX
    QSocketNotifier* unixSignalNotifier = 0;
    if (setupUnixTerminationSignalHandlers()) {
        unixSignalNotifier = new QSocketNotifier(g_unixSignalFds[1], QSocketNotifier::Read, &application);
        QObject::connect(unixSignalNotifier, &QSocketNotifier::activated, &application, [&]() {
            char signalCode = 0;
            const ssize_t bytesRead = ::read(g_unixSignalFds[1], &signalCode, sizeof(signalCode));
            if (bytesRead > 0)
                requestGracefulShutdown(Qt::QueuedConnection);
        });
    } else {
        qWarning() << "Could not install Unix termination signal handlers.";
    }
#endif

#ifdef Q_OS_WIN
    if (!SetConsoleCtrlHandler(handleWindowsConsoleControl, TRUE))
        qWarning() << "Could not install Windows console termination handler.";
#endif

    const QString serverName = singletonServerName();
    if (notifyRunningInstance(serverName))
        return 0;

    bool alreadyRunning = false;
    if (startSingletonServer(&singletonServer, serverName, &alreadyRunning)) {
        QObject::connect(&singletonServer, &QLocalServer::newConnection, &application, [&]() {
            bool acceptedActivation = false;
            while (singletonServer.hasPendingConnections()) {
                QLocalSocket* socket = singletonServer.nextPendingConnection();
                if (!socket)
                    continue;
                if (shutdownInProgress()) {
                    // Refuse activation during shutdown so the launching
                    // process removes the stale server and performs a full
                    // restore of all saved windows.
                    socket->write("shutting-down\n");
                    socket->waitForBytesWritten(250);
                    socket->close();
                    socket->deleteLater();
                    continue;
                }

                socket->write("accepted\n");
                socket->waitForBytesWritten(250);
                socket->close();
                socket->deleteLater();
                acceptedActivation = true;
            }

            // If all pending sockets were refused because shutdown is in
            // progress, do not raise a window that is about to disappear.
            if (!acceptedActivation)
                return;
            if (g_mainWindow)
                activateMainWindow(g_mainWindow);
            else
                g_pendingActivation.store(true, std::memory_order_release);
        });
    } else if (alreadyRunning) {
        return 0;
    }

    MainWindow window;
    g_mainWindow = &window;
    window.show();
#ifdef Q_OS_WIN
    GlobalFocusHotkey globalFocusHotkey(&window);
    application.installNativeEventFilter(&globalFocusHotkey);
#endif

    if (g_pendingActivation.exchange(false, std::memory_order_acq_rel))
        activateMainWindow(g_mainWindow);

    QObject::connect(&application, &QGuiApplication::lastWindowClosed, &application, [&]() {
        if (!shutdownInProgress())
            application.quit();
    });

    g_eventLoopRunning.store(true);
    const int result = application.exec();
    g_eventLoopRunning.store(false);
    g_mainWindow = 0;
    return result;
}
