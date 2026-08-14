#if defined(_WIN32)

#include "WindowsService.hpp"
#include "Application.hpp"

#include <boost/asio.hpp>
#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace {
constexpr wchar_t SERVICE_NAME[] = L"HydrogenHttpd";
constexpr wchar_t SERVICE_DISPLAY_NAME[] = L"HydrogenHttpd Web Server";

SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
SERVICE_STATUS g_status{};
boost::asio::io_context* g_io = nullptr;
std::filesystem::path g_serviceConfig = L"C:\\ProgramData\\HydrogenHttpd\\server.conf";

void reportStatus(DWORD state, DWORD exitCode = NO_ERROR, DWORD waitHint = 0) {
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState = state;
    g_status.dwWin32ExitCode = exitCode;
    g_status.dwWaitHint = waitHint;
    g_status.dwControlsAccepted =
        state == SERVICE_START_PENDING ? 0 : SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    static DWORD checkpoint = 1;
    g_status.dwCheckPoint =
        (state == SERVICE_RUNNING || state == SERVICE_STOPPED) ? 0 : checkpoint++;

    if (g_statusHandle) SetServiceStatus(g_statusHandle, &g_status);
}

void WINAPI serviceControlHandler(DWORD control) {
    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN) {
        reportStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
        if (g_io) g_io->stop();
    }
}

void WINAPI serviceMain(DWORD argc, LPWSTR* argv) {
    g_statusHandle = RegisterServiceCtrlHandlerW(SERVICE_NAME, serviceControlHandler);
    if (!g_statusHandle) return;

    reportStatus(SERVICE_START_PENDING, NO_ERROR, 5000);
    (void)argc;
    (void)argv;
    const auto configPath = g_serviceConfig;

    reportStatus(SERVICE_RUNNING);
    const int result = runHydrogenHttpd(configPath, [](boost::asio::io_context& io) {
        g_io = &io;
    });
    g_io = nullptr;

    reportStatus(SERVICE_STOPPED, result == 0 ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR);
}

std::wstring quote(const std::filesystem::path& path) {
    return L"\"" + path.wstring() + L"\"";
}
}

int runHydrogenHttpdWindowsService(const std::filesystem::path& configPath) {
    g_serviceConfig = std::filesystem::absolute(configPath);
    SERVICE_TABLE_ENTRYW table[] = {
        {const_cast<LPWSTR>(SERVICE_NAME), serviceMain},
        {nullptr, nullptr}
    };

    if (!StartServiceCtrlDispatcherW(table)) {
        std::cerr << "StartServiceCtrlDispatcher failed: " << GetLastError() << "\n";
        return 1;
    }
    return 0;
}

int installHydrogenHttpdWindowsService(
    const std::filesystem::path& executable,
    const std::filesystem::path& configPath
) {
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!manager) {
        std::cerr << "OpenSCManager failed: " << GetLastError() << "\n";
        return 1;
    }

    const std::wstring command =
        quote(std::filesystem::absolute(executable)) +
        L" --service --config " +
        quote(std::filesystem::absolute(configPath));

    SC_HANDLE service = CreateServiceW(
        manager,
        SERVICE_NAME,
        SERVICE_DISPLAY_NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        command.c_str(),
        nullptr,
        nullptr,
        nullptr,
        L"NT AUTHORITY\\LocalService",
        nullptr
    );

    if (!service && GetLastError() == ERROR_SERVICE_EXISTS) {
        service = OpenServiceW(manager, SERVICE_NAME, SERVICE_ALL_ACCESS);
        if (service) {
            ChangeServiceConfigW(
                service,
                SERVICE_NO_CHANGE,
                SERVICE_AUTO_START,
                SERVICE_NO_CHANGE,
                command.c_str(),
                nullptr,
                nullptr,
                nullptr,
                L"NT AUTHORITY\\LocalService",
                nullptr,
                SERVICE_DISPLAY_NAME
            );
        }
    }

    if (!service) {
        std::cerr << "Create/Open service failed: " << GetLastError() << "\n";
        CloseServiceHandle(manager);
        return 1;
    }

    SERVICE_DESCRIPTIONW description{};
    description.lpDescription = const_cast<LPWSTR>(
        L"High-performance experimental HTTP/HTTPS server."
    );
    ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &description);

    SERVICE_FAILURE_ACTIONS failure{};
    SC_ACTION actions[3] = {
        {SC_ACTION_RESTART, 5000},
        {SC_ACTION_RESTART, 15000},
        {SC_ACTION_RESTART, 60000}
    };
    failure.dwResetPeriod = 86400;
    failure.cActions = 3;
    failure.lpsaActions = actions;
    ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS, &failure);

    CloseServiceHandle(service);
    CloseServiceHandle(manager);
    return 0;
}

int uninstallHydrogenHttpdWindowsService() {
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!manager) return 1;

    SC_HANDLE service = OpenServiceW(manager, SERVICE_NAME, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
    if (!service) {
        const DWORD error = GetLastError();
        CloseServiceHandle(manager);
        return error == ERROR_SERVICE_DOES_NOT_EXIST ? 0 : 1;
    }

    SERVICE_STATUS status{};
    ControlService(service, SERVICE_CONTROL_STOP, &status);
    const BOOL removed = DeleteService(service);

    CloseServiceHandle(service);
    CloseServiceHandle(manager);
    return removed ? 0 : 1;
}

#endif
