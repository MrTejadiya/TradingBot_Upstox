#pragma once

#include "tradingbot/infra/upstox_api_client.hpp"

namespace tradingbot::infra {

class WinHttpTransport final : public HttpTransport {
public:
    HttpResponse send(const HttpRequest& request) override;
};

}  // namespace tradingbot::infra
