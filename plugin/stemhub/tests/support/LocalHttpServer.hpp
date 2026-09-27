#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include <JuceHeader.h>

namespace stemhub::test
{
// Serves one canned response per request on 127.0.0.1 and records what it received.
class LocalHttpServer final : private juce::Thread
{
public:
    struct Request
    {
        juce::String requestLine;
        juce::String headers;
    };

    using Handler = std::function<juce::String(const Request&)>;

    explicit LocalHttpServer(Handler handlerToUse)
        : juce::Thread("Test HTTP server"), handler(std::move(handlerToUse))
    {
        if (listener.createListener(0, "127.0.0.1"))
            startThread();
    }

    ~LocalHttpServer() override
    {
        signalThreadShouldExit();
        listener.close();
        stopThread(2000);
    }

    juce::String getBaseUrl() const
    {
        return "http://127.0.0.1:" + juce::String(listener.getBoundPort());
    }

    std::vector<Request> getRequests() const
    {
        const std::lock_guard<std::mutex> lock(requestsMutex);
        return requests;
    }

private:
    void run() override
    {
        while (!threadShouldExit())
        {
            std::unique_ptr<juce::StreamingSocket> connection(listener.waitForNextConnection());
            // Closing the listener connects to it once to wake this thread up.
            if (connection == nullptr || threadShouldExit())
                return;

            juce::MemoryOutputStream received;
            char buffer[1024];
            while (!received.toString().contains("\r\n\r\n") && connection->waitUntilReady(true, 2000) == 1)
            {
                const auto bytesRead = connection->read(buffer, sizeof(buffer), false);
                if (bytesRead <= 0)
                    break;
                received.write(buffer, static_cast<size_t>(bytesRead));
            }

            const auto text = received.toString();
            if (text.isEmpty())
                continue;

            Request request { text.upToFirstOccurrenceOf("\r\n", false, false),
                              text.fromFirstOccurrenceOf("\r\n", false, false) };
            {
                const std::lock_guard<std::mutex> lock(requestsMutex);
                requests.push_back(request);
            }

            const auto response = handler(request);
            connection->write(response.toRawUTF8(), static_cast<int>(response.getNumBytesAsUTF8()));
        }
    }

    Handler handler;
    juce::StreamingSocket listener;
    mutable std::mutex requestsMutex;
    std::vector<Request> requests;
};
}
