#include "utils/ipa_update.h"
#include "resource/resource.h"
#include <shellapi.h>
#include <winhttp.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace PhoneticIpa::Updates
{
namespace
{
constexpr wchar_t kKey[] = L"Software\\PhoneticIpa\\Updates";
constexpr wchar_t kDownloadPageUrl[] = L"https://www.phonetictoolbox.com/#ipa-ime-download";
constexpr UINT kUpdateCheckDoneMessage = WM_APP + 2;
std::thread monitor;
HWND monitor_window = nullptr;
std::thread check_thread;
std::atomic<bool> check_in_flight{false};
std::atomic<bool> stop_requested{false};

struct ActiveHandle
{
    std::mutex mutex;
    HINTERNET value = nullptr;
    explicit ActiveHandle(HINTERNET handle) : value(handle)
    {
    }
    void Close()
    {
        HINTERNET raw = nullptr;
        {
            std::lock_guard lock(mutex);
            raw = std::exchange(value, nullptr);
        }
        if (raw)
            WinHttpCloseHandle(raw);
    }
    HINTERNET Raw()
    {
        std::lock_guard lock(mutex);
        return value;
    }
    ~ActiveHandle()
    {
        Close();
    }
};

std::mutex g_requests_mutex;
std::vector<std::weak_ptr<ActiveHandle>> g_active_requests;

std::shared_ptr<ActiveHandle> TrackRequestHandle(HINTERNET raw)
{
    if (!raw)
        return {};
    auto tracked = std::make_shared<ActiveHandle>(raw);
    std::lock_guard lock(g_requests_mutex);
    if (stop_requested.load())
    {
        tracked->Close();
        return {};
    }
    g_active_requests.erase(std::remove_if(g_active_requests.begin(), g_active_requests.end(),
                                           [](const std::weak_ptr<ActiveHandle> &entry) { return entry.expired(); }),
                            g_active_requests.end());
    g_active_requests.push_back(tracked);
    return tracked;
}

void AbortActiveRequests()
{
    std::vector<std::shared_ptr<ActiveHandle>> snapshot;
    {
        std::lock_guard lock(g_requests_mutex);
        for (const auto &weak : g_active_requests)
            if (auto locked = weak.lock())
                snapshot.push_back(std::move(locked));
        g_active_requests.clear();
    }
    for (const auto &handle : snapshot)
        handle->Close();
}

std::wstring Wide(const std::string &text)
{
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), (int)text.size(), nullptr, 0);
    std::wstring value(size, 0);
    if (size)
        MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), value.data(), size);
    return value;
}
std::string Read(const wchar_t *name)
{
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_BINARY, nullptr, nullptr, &size) != ERROR_SUCCESS ||
        size > 65536)
        return {};
    std::string value(size, 0);
    if (RegGetValueW(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_BINARY, nullptr, value.data(), &size) != ERROR_SUCCESS)
        return {};
    value.resize(size);
    return value;
}
void Write(const wchar_t *name, const std::string &value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExW(key, name, 0, REG_BINARY, reinterpret_cast<const BYTE *>(value.data()), (DWORD)value.size());
        RegCloseKey(key);
    }
}
int64_t Time(const wchar_t *name)
{
    try
    {
        return std::stoll(Read(name));
    }
    catch (...)
    {
        return 0;
    }
}
std::optional<Feed> Fetch(const char *endpoint)
{
    if (stop_requested.load())
        return {};
    std::wstring url = Wide(endpoint);
    URL_COMPONENTS parts{sizeof(parts)};
    wchar_t host[256]{}, path[2048]{};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS)
        return {};
    const auto agent = L"PhoneticIPA/" + Wide(kVersion);
    auto session =
        TrackRequestHandle(WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0));
    if (!session || !session->Raw())
        return {};
    WinHttpSetTimeouts(session->Raw(), 5000, 5000, 8000, 8000);
    auto connection = TrackRequestHandle(WinHttpConnect(session->Raw(), host, parts.nPort, 0));
    if (!connection || !connection->Raw())
        return {};
    auto request = TrackRequestHandle(WinHttpOpenRequest(connection->Raw(), L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                                         WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request || !request->Raw())
        return {};
    DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES;
    WinHttpSetOption(request->Raw(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled));
    DWORD status = 0, size = sizeof(status);
    if (!WinHttpSendRequest(request->Raw(), L"Accept: application/json\r\n", static_cast<DWORD>(-1L), nullptr, 0, 0,
                            0) ||
        !WinHttpReceiveResponse(request->Raw(), nullptr) ||
        !WinHttpQueryHeaders(request->Raw(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status,
                             &size, nullptr) ||
        status != 200)
        return {};
    std::string body;
    char buffer[4096];
    DWORD count;
    auto start = std::chrono::steady_clock::now();
    while (!stop_requested.load())
    {
        if (!WinHttpReadData(request->Raw(), buffer, sizeof(buffer), &count))
            return {};
        if (!count)
            return Parse(body);
        if (body.size() + count > 65536 || std::chrono::steady_clock::now() - start > std::chrono::seconds(12))
            return {};
        body.append(buffer, count);
    }
    return {};
}
UINT NextCheckDelayMs()
{
    const auto now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto last_success = Time(L"Success");
    const auto last_attempt = Time(L"Attempt");
    const auto failures = Time(L"Failures");
    long long wait_seconds = 0;
    if (last_success > 0 && last_success <= now)
    {
        wait_seconds = (std::max)(wait_seconds, kCheckIntervalSeconds - (now - last_success));
    }
    if (failures > 0 && last_attempt > 0 && last_attempt <= now)
    {
        wait_seconds = (std::max)(wait_seconds, RetryDelaySeconds(failures - 1) - (now - last_attempt));
    }
    const long long ms = std::clamp(wait_seconds * 1000LL, 15000LL, 3600000LL);
    return static_cast<UINT>(ms);
}
LRESULT CALLBACK MonitorProc(HWND window, UINT message, WPARAM w, LPARAM l)
{
    if (message == WM_TIMER)
    {
        KillTimer(window, 1);
        if (stop_requested.load())
            return 0;
        if (!check_in_flight.exchange(true))
        {
            if (check_thread.joinable())
                check_thread.join();
            check_thread = std::thread([window] {
                auto *result = new Result(Check(false));
                if (!PostMessageW(window, kUpdateCheckDoneMessage, 0, reinterpret_cast<LPARAM>(result)))
                    delete result;
            });
        }
        return 0;
    }
    if (message == kUpdateCheckDoneMessage)
    {
        std::unique_ptr<Result> result(reinterpret_cast<Result *>(l));
        check_in_flight.store(false);
        if (check_thread.joinable())
            check_thread.join();
        if (!stop_requested.load() && result)
        {
            if (Automatic() && result->latest && Read(L"Notified") != result->latest->version)
            {
                NOTIFYICONDATAW icon{sizeof(icon)};
                icon.hWnd = window;
                icon.uID = 1;
                icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
                icon.uCallbackMessage = WM_APP + 1;
                icon.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_IME_ICON));
                wcscpy_s(icon.szTip, L"国际音标输入法 · 更新");
                if (Shell_NotifyIconW(NIM_ADD, &icon))
                {
                    icon.uFlags = NIF_INFO;
                    icon.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
                    wcscpy_s(icon.szInfoTitle, L"国际音标输入法有新版本");
                    std::wstring text = L"版本 " + Wide(result->latest->version) + L" 已发布。点击前往官网下载。";
                    wcsncpy_s(icon.szInfo, text.c_str(), _TRUNCATE);
                    if (Shell_NotifyIconW(NIM_MODIFY, &icon))
                        Write(L"Notified", result->latest->version);
                }
            }
            SetTimer(window, 1, NextCheckDelayMs(), nullptr);
        }
        return 0;
    }
    if (message == WM_APP + 1)
    {
        if (l == NIN_BALLOONUSERCLICK || l == WM_LBUTTONUP)
            if (Available())
                ShellExecuteW(nullptr, L"open", kDownloadPageUrl, nullptr, nullptr, SW_SHOWNORMAL);
        if (l == NIN_BALLOONUSERCLICK || l == WM_LBUTTONUP || l == NIN_BALLOONTIMEOUT || l == NIN_BALLOONHIDE)
        {
            NOTIFYICONDATAW icon{sizeof(icon)};
            icon.hWnd = window;
            icon.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &icon);
        }
        return 0;
    }
    if (message == WM_CLOSE)
    {
        KillTimer(window, 1);
        stop_requested.store(true);
        AbortActiveRequests();
        if (check_thread.joinable())
            check_thread.join();
        check_in_flight.store(false);
        MSG pending{};
        while (PeekMessageW(&pending, window, kUpdateCheckDoneMessage, kUpdateCheckDoneMessage, PM_REMOVE))
            delete reinterpret_cast<Result *>(pending.lParam);
        NOTIFYICONDATAW icon{sizeof(icon)};
        icon.hWnd = window;
        icon.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &icon);
        DestroyWindow(window);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}
} // namespace
bool Automatic()
{
    return Read(L"Automatic") != "0";
}
void SetAutomatic(bool enabled)
{
    Write(L"Automatic", enabled ? "1" : "0");
}
std::optional<Feed> Available()
{
    auto feed = Parse(Read(L"Latest"));
    return feed && *ParseVersion(feed->version) > *ParseVersion(kVersion) ? feed : std::nullopt;
}
Result Check(bool manual)
{
    auto latest = Available();
    auto now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    Result cached{latest, latest ? L"发现新版本 " + Wide(latest->version) : L"自动检查官网与 GitHub 的新版本"};
    auto last_success = Time(L"Success");
    auto last_attempt = Time(L"Attempt");
    const auto failures = Time(L"Failures");
    if (last_success > now)
    {
        last_success = 0;
        Write(L"Success", "0");
    }
    if (last_attempt > now)
    {
        last_attempt = 0;
        Write(L"Attempt", "0");
    }
    const long long retry_delay = failures > 0 ? RetryDelaySeconds(failures - 1) : 0;
    if (!manual && (!Automatic() || (last_success > 0 && now - last_success < kCheckIntervalSeconds) ||
                    (retry_delay > 0 && last_attempt > 0 && now - last_attempt < retry_delay)))
        return cached;
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\PhoneticIpa_UpdateCheck");
    if (!mutex)
        return cached;
    DWORD locked = WaitForSingleObject(mutex, 0);
    if (locked != WAIT_OBJECT_0 && locked != WAIT_ABANDONED)
    {
        CloseHandle(mutex);
        return {latest, L"正在检查官网与 GitHub…"};
    }
    Write(L"Attempt", std::to_string(now));
    std::vector<Feed> feeds;
    try
    {
        auto server = std::async(std::launch::async, Fetch, kServer);
        auto github = std::async(std::launch::async, Fetch, kGithub);
        if (auto feed = server.get())
            feeds.push_back(*feed);
        if (auto feed = github.get())
            feeds.push_back(*feed);
    }
    catch (const std::exception &)
    {
    }
    if (!feeds.empty())
    {
        Write(L"Success", std::to_string(now));
        Write(L"Failures", "0");
    }
    else if (!stop_requested.load())
        Write(L"Failures", std::to_string(failures + 1));
    if (auto best = Newest(feeds); best && *ParseVersion(best->version) > *ParseVersion(kVersion) &&
                                   (!latest || *ParseVersion(best->version) >= *ParseVersion(latest->version)))
    {
        latest = best;
        Write(L"Latest", best->json);
    }
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return {latest, latest              ? L"发现新版本 " + Wide(latest->version)
                    : feeds.empty()     ? L"暂时无法连接更新服务，请稍后重试"
                    : feeds.size() == 2 ? L"已是最新版本"
                                        : L"已检查一个来源，另一来源暂时无法连接"};
}
void StartMonitor()
{
    if (monitor.joinable())
        return;
    stop_requested.store(false);
    check_in_flight.store(false);
    std::promise<HWND> ready;
    auto future = ready.get_future();
    monitor = std::thread([ready = std::move(ready)]() mutable {
        WNDCLASSW type{};
        type.lpfnWndProc = MonitorProc;
        type.hInstance = GetModuleHandleW(nullptr);
        type.lpszClassName = L"PhoneticIpaUpdateMonitor";
        RegisterClassW(&type);
        HWND window =
            CreateWindowW(type.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, type.hInstance, nullptr);
        ready.set_value(window);
        if (!window)
            return;
        SetTimer(window, 1, 15000, nullptr);
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    });
    monitor_window = future.get();
}
void StopMonitor()
{
    stop_requested.store(true);
    AbortActiveRequests();
    if (monitor_window)
        PostMessageW(monitor_window, WM_CLOSE, 0, 0);
    if (monitor.joinable())
        monitor.join();
    monitor_window = nullptr;
}
} // namespace PhoneticIpa::Updates
