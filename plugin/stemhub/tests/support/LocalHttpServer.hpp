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
        // As much of it as Content-Length announced.
        juce::String body;
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
            const auto readMore = [&connection, &received]
            {
                char buffer[1024];
                if (connection->waitUntilReady(true, 2000) != 1)
                    return false;

                const auto bytesRead = connection->read(buffer, sizeof(buffer), false);
                if (bytesRead <= 0)
                    return false;

                received.write(buffer, static_cast<size_t>(bytesRead));
                return true;
            };

            while (!received.toString().contains("\r\n\r\n") && readMore())
            {
            }

            const auto head = received.toString().upToFirstOccurrenceOf("\r\n\r\n", false, false);
            if (head.isEmpty())
                continue;

            // The body follows the blank line, as long as Content-Length says.
            const auto bodyStart = juce::jmin(head.getNumBytesAsUTF8() + 4, received.getDataSize());
            const auto bodyLength = static_cast<size_t>(juce::jmax(0, contentLengthOf(head)));
            while (received.getDataSize() < bodyStart + bodyLength && readMore())
            {
            }

            const auto* bodyData = static_cast<const char*>(received.getData()) + bodyStart;
            const auto bodyBytes = juce::jmin(bodyLength, received.getDataSize() - bodyStart);
            Request request { head.upToFirstOccurrenceOf("\r\n", false, false),
                              head.fromFirstOccurrenceOf("\r\n", false, false),
                              juce::String::fromUTF8(bodyData, static_cast<int>(bodyBytes)) };
            {
                const std::lock_guard<std::mutex> lock(requestsMutex);
                requests.push_back(request);
            }

            const auto response = handler(request);
            connection->write(response.toRawUTF8(), static_cast<int>(response.getNumBytesAsUTF8()));
        }
    }

    static int contentLengthOf(const juce::String& head)
    {
        juce::StringArray lines;
        lines.addLines(head);
        for (const auto& line : lines)
            if (line.startsWithIgnoreCase("Content-Length:"))
                return line.fromFirstOccurrenceOf(":", false, false).trim().getIntValue();

        return 0;
    }

    Handler handler;
    juce::StreamingSocket listener;
    mutable std::mutex requestsMutex;
    std::vector<Request> requests;
};
}
