#include "RocgalgameInputTransport.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
bool WaitFor(const std::function<bool()> &condition,
             std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while(std::chrono::steady_clock::now() < deadline) {
        if(condition())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return condition();
}

void WriteLine(int fd, const std::string &line) {
    const ssize_t written = write(fd, line.data(), line.size());
    assert(written == static_cast<ssize_t>(line.size()));
}
}

int main() {
    const auto fifo = std::filesystem::temp_directory_path() /
        ("rocgalgame-krkr2-transport-" + std::to_string(getpid()) + ".fifo");
    unlink(fifo.c_str());
    assert(mkfifo(fifo.c_str(), 0600) == 0);

    int writer = open(fifo.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    assert(writer >= 0);
    RocgalgameInputTransport transport;
    assert(transport.start(fifo.string()));

    WriteLine(writer, "A 0.5 -0.25 100\n");
    assert(WaitFor([&] {
        const auto snapshot = transport.drain();
        return snapshot.connected && snapshot.axisSequence == 100 &&
               snapshot.axisX == 0.5f && snapshot.axisY == -0.25f;
    }, std::chrono::seconds(2)));

    close(writer);
    assert(WaitFor([&] {
        return !transport.drain().connected;
    }, std::chrono::seconds(2)));

    writer = open(fifo.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    assert(writer >= 0);
    WriteLine(writer, "A -0.75 0.25 1\n");
    assert(WaitFor([&] {
        const auto snapshot = transport.drain();
        return snapshot.connected && snapshot.axisSequence == 1 &&
               snapshot.axisX == -0.75f && snapshot.axisY == 0.25f;
    }, std::chrono::seconds(2)));

    close(writer);
    transport.stop();
    unlink(fifo.c_str());
    return 0;
}
