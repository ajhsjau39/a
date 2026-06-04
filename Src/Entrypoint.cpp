#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include "libraries/RBX/TeleportHandler/TeleportHandler.hpp"
#include "libraries/RBX/TaskScheduler/TaskScheduler.hpp"

#define HORIZON_PORT 9999

static void* TCPServer(void*) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return nullptr;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(HORIZON_PORT);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) return nullptr;
    listen(server_fd, 5);

    printf("[Horizon] Listening on port %d\n", HORIZON_PORT);

    while (true) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) continue;

        std::string script;
        char buf[4096];
        ssize_t n;
        while ((n = recv(client, buf, sizeof(buf), 0)) > 0)
            script.append(buf, n);
        close(client);

        if (!script.empty()) {
            printf("[Horizon] Executing script (%zu bytes)\n", script.size());
            RBX::TaskScheduler::SendScript(script);
        }
    }
    return nullptr;
}

__attribute__((constructor))
static void Init() {
    printf("[Horizon] Injected!\n");

    pthread_t t1, t2;

    pthread_create(&t1, nullptr, [](void*) -> void* {
        RBX::TeleportHandler::Initialize();
        return nullptr;
    }, nullptr);
    pthread_detach(t1);

    pthread_create(&t2, nullptr, TCPServer, nullptr);
    pthread_detach(t2);
}