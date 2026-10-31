// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#pragma once

namespace httplib
{
class Server;
struct Request;
struct Response;
} // namespace httplib

class ServiceHandler
{
public:
    void registerRoutes(httplib::Server& server) const;

private:
    void handleOffsets(const httplib::Request& req,
                       httplib::Response& res) const;
};
