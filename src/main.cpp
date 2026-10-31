// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include <iostream>

#include "ServiceHandler.h"
#include "httplib.h"

int main()
{
    httplib::Server server;
    ServiceHandler handler;
    handler.registerRoutes(server);

    std::cout << "C++ RPY server starting...\n"
              << "Rotation endpoint: http://localhost:8080/api/offsets\n";

    server.set_mount_point("/", "./www");

    if (!server.listen("127.0.0.1", 8080))
    {
        std::cerr << "Error: Could not start server on port 8080.\n";

        return 1;
    }

    return 0;
}