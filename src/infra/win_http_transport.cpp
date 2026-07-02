#include "tradingbot/infra/win_http_transport.hpp"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

#include <stdexcept>
#include <string>

namespace tradingbot::infra {
namespace {

#ifdef _WIN32
std::wstring widen(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const auto size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        throw std::runtime_error("failed to convert UTF-8 string to wide string");
    }
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size);
    return out;
}

std::string narrow(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const auto size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        throw std::runtime_error("failed to convert wide string to UTF-8 string");
    }
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size, nullptr, nullptr);
    return out;
}

class Handle {
public:
    explicit Handle(HINTERNET handle = nullptr) : handle_(handle) {}
    ~Handle() {
        if (handle_ != nullptr) {
            WinHttpCloseHandle(handle_);
        }
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HINTERNET get() const {
        return handle_;
    }

private:
    HINTERNET handle_{nullptr};
};

[[noreturn]] void fail_last_error(const std::string& message) {
    throw std::runtime_error(message + ": " + std::to_string(GetLastError()));
}
#endif

}  // namespace

HttpResponse WinHttpTransport::send(const HttpRequest& request) {
#ifndef _WIN32
    (void)request;
    throw std::runtime_error("WinHttpTransport is only available on Windows");
#else
    const auto url = widen(request.url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts)) {
        fail_last_error("failed to parse URL");
    }

    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

    Handle session(WinHttpOpen(L"TradingBotUpstox/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (session.get() == nullptr) {
        fail_last_error("failed to open WinHTTP session");
    }
    Handle connection(WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0));
    if (connection.get() == nullptr) {
        fail_last_error("failed to connect");
    }

    const auto method = widen(request.method);
    const auto flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    Handle http_request(WinHttpOpenRequest(connection.get(), method.c_str(), path.c_str(), nullptr,
                                          WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (http_request.get() == nullptr) {
        fail_last_error("failed to open HTTP request");
    }

    const DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    WinHttpSetOption(http_request.get(), WINHTTP_OPTION_DECOMPRESSION,
                     const_cast<DWORD*>(&decompression), sizeof(decompression));

    std::wstring headers;
    for (const auto& [name, value] : request.headers) {
        headers += widen(name + ": " + value + "\r\n");
    }

    auto* body = request.body.empty() ? WINHTTP_NO_REQUEST_DATA :
        reinterpret_cast<LPVOID>(const_cast<char*>(request.body.data()));
    const auto body_size = static_cast<DWORD>(request.body.size());
    if (!WinHttpSendRequest(http_request.get(), headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                            static_cast<DWORD>(headers.size()), body, body_size, body_size, 0)) {
        fail_last_error("failed to send HTTP request");
    }
    if (!WinHttpReceiveResponse(http_request.get(), nullptr)) {
        fail_last_error("failed to receive HTTP response");
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (!WinHttpQueryHeaders(http_request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX)) {
        fail_last_error("failed to read HTTP status");
    }

    HttpResponse response;
    response.status_code = static_cast<int>(status_code);

    DWORD header_size = 0;
    WinHttpQueryHeaders(http_request.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr,
                        &header_size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && header_size > 0) {
        std::wstring raw_headers(header_size / sizeof(wchar_t), L'\0');
        if (WinHttpQueryHeaders(http_request.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                                raw_headers.data(), &header_size, WINHTTP_NO_HEADER_INDEX)) {
            response.headers.emplace("raw", narrow(raw_headers));
        }
    }

    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(http_request.get(), &available)) {
            fail_last_error("failed to query response body");
        }
        if (available == 0) {
            break;
        }
        const auto offset = response.body.size();
        response.body.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(http_request.get(), response.body.data() + offset, available, &read)) {
            fail_last_error("failed to read response body");
        }
        response.body.resize(offset + read);
    }

    return response;
#endif
}

}  // namespace tradingbot::infra
