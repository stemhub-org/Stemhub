#include <memory>

#include "network/ApiClient.hpp"
#include "network/ApiConfig.hpp"
#include "network/ApiJson.hpp"
#include "support/LocalHttpServer.hpp"
#include "support/TestSupport.hpp"

namespace
{
using namespace stemhub::test;

class ApiTests final : public StemhubTest
{
public:
    ApiTests() : StemhubTest("Stemhub API") {}

    void runTest() override
    {
        beginTest("API responses are parsed into domain values");
        {
            namespace json = stemhub::api::json;

            const auto versions = json::parseVersions(juce::JSON::parse(R"([{
                "id": "v1", "branch_id": "b1", "created_at": "2026-03-18T10:00:00Z", "message": "first",
                "manifest_json": { "manifest_version": 2, "project_file": { "size_bytes": 10 },
                                   "assets": [ { "size_bytes": 5 }, { "size_bytes": 7 } ] }
            }])"));
            expect(versions.ok() && versions.value->size() == 1, "a version list parses");
            if (versions.ok())
            {
                expect(versions.value->front().message == "first", "the version's message is read");
                expect(versions.value->front().totalSizeBytes == 22, "the size is summed from the manifest");
            }

            const auto olderBackend = json::parseVersionSummary(juce::JSON::parse(R"({
                "id": "v1", "branch_id": "b1", "commit_message": "from an older backend",
                "manifest_json": { "manifest_version": 1, "project_file": { "size_bytes": 10 }, "tracks": [ { "size_bytes": 5 } ] }
            })"));
            expect(olderBackend.ok() && olderBackend.value->message == "from an older backend",
                   "a backend that still says commit_message is understood");
            expect(olderBackend.ok() && olderBackend.value->totalSizeBytes == 15, "and so is a v1 manifest's size");

            const auto withoutMessage = json::parseVersionSummary(juce::JSON::parse(R"({
                "id": "v1", "branch_id": "b1", "message": null, "commit_message": "stale"
            })"));
            expect(withoutMessage.ok() && withoutMessage.value->message.isEmpty(), "a null message means none");

            expect(!json::parseVersions(juce::JSON::parse(R"({"id": "v1"})")).ok(), "an object is not a list");
            const auto missingFields = json::parseProject(juce::JSON::parse(R"({"id": "p1"})"));
            expect(!missingFields.ok() && missingFields.error->kind == ApiError::Kind::invalidResponse,
                   "a project without a name is invalid");
            expect(json::parseMissingBlobs(juce::JSON::parse(R"({"missing": ["a", "b"]})")).value->size() == 2);
            expect(!json::parseLogin(juce::JSON::parse(R"({"token_type": "bearer"})")).ok(), "a login without a token fails");

            expect(json::extractErrorMessage(juce::JSON::parse(R"({"detail": "Incorrect email or password"})"), {}, "x")
                       == "Incorrect email or password");
            expect(json::extractErrorMessage(juce::JSON::parse(R"({"detail": [{"msg": "field required"}]})"), {}, "x")
                       == "field required");
            expect(json::extractErrorMessage({}, "<html>Bad gateway</html>", "Request failed.") == "Request failed.",
                   "HTML error pages are not shown");

            expect(ApiError::kindForStatus(401) == ApiError::Kind::unauthorized);
            expect(ApiError::kindForStatus(422) == ApiError::Kind::invalidRequest);
            expect(ApiError::kindForStatus(503) == ApiError::Kind::server);
            expect(ApiError::kindForStatus(0) == ApiError::Kind::network);
        }

        beginTest("The API base URL is https, or http to this machine");
        {
            using stemhub::api::chooseBaseUrl;
            using stemhub::api::normaliseBaseUrl;

            expect(normaliseBaseUrl("https://api.stemhub.app/") == "https://api.stemhub.app");
            expect(normaliseBaseUrl(" http://localhost:8000 ") == "http://localhost:8000");
            expect(normaliseBaseUrl("http://127.0.0.1:8000/") == "http://127.0.0.1:8000");
            expect(normaliseBaseUrl("http://[::1]:8000") == "http://[::1]:8000");
            expect(normaliseBaseUrl("http://api.stemhub.app").isEmpty(), "plain http to another host would leak the token");
            expect(normaliseBaseUrl("http://localhost.evil.example").isEmpty());
            expect(normaliseBaseUrl("http://localhost@evil.example").isEmpty());
            expect(normaliseBaseUrl("ftp://api.stemhub.app").isEmpty());
            expect(normaliseBaseUrl("https://").isEmpty());

            const juce::String builtIn = "http://localhost:8000/";
            const juce::String configFile = R"({ "api_base_url": "https://staging.stemhub.app" })";
            expect(chooseBaseUrl({}, {}, builtIn) == "http://localhost:8000", "the built-in URL is the fallback");
            expect(chooseBaseUrl({}, configFile, builtIn) == "https://staging.stemhub.app", "the config file overrides it");
            expect(chooseBaseUrl("https://dev.stemhub.app", configFile, builtIn) == "https://dev.stemhub.app",
                   "the environment wins");
            expect(chooseBaseUrl("http://evil.example", "not json", builtIn) == "http://localhost:8000",
                   "unusable overrides are ignored");
        }

        beginTest("Blob downloads follow the storage redirect without the bearer token");
        {
            const juce::String storagePath = "/storage/blob?X-Goog-Signature=ab%2Fcd&X-Goog-Credential=a%40b";
            std::unique_ptr<LocalHttpServer> server;
            server = std::make_unique<LocalHttpServer>([&server, storagePath](const LocalHttpServer::Request& request)
            {
                if (request.requestLine.startsWith("GET /projects/"))
                    return "HTTP/1.1 307 Temporary Redirect\r\nLocation: " + server->getBaseUrl() + storagePath
                         + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";

                return juce::String("HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: close\r\n\r\nbytes");
            });

            TestEnvironment environment;
            const auto destination = environment.root.getChildFile("blob.bin");
            ApiClient client(server->getBaseUrl());
            const auto result = client.downloadBlob("project-1", juce::String::repeatedString("c", 64), destination, "secret-token");

            expect(result.ok(), result.errorMessage("download failed"));
            expect(destination.loadFileAsString() == "bytes", "the storage response is written to disk");

            const auto requests = server->getRequests();
            expect(requests.size() == 2, "one request to the API, one to storage");
            if (requests.size() == 2)
            {
                expect(requests[0].headers.contains("Bearer secret-token"), "the API request is authorized");
                expect(requests[1].requestLine == "GET " + storagePath + " HTTP/1.1",
                       "the presigned URL is requested as-is: " + requests[1].requestLine);
                expect(!requests[1].headers.containsIgnoreCase("authorization"), "the token never reaches storage");
            }
        }

        beginTest("A new version is sent with its message, and without one when the user wrote none");
        {
            LocalHttpServer server([](const LocalHttpServer::Request& request)
            {
                juce::ignoreUnused(request);
                const juce::String body = R"({"id": "v1", "branch_id": "b1"})";
                return "HTTP/1.1 201 Created\r\nContent-Type: application/json\r\nContent-Length: "
                     + juce::String(static_cast<int>(body.getNumBytesAsUTF8())) + "\r\nConnection: close\r\n\r\n" + body;
            });

            ApiClient client(server.getBaseUrl());
            CreateVersionRequest request;
            request.message = "Drums";
            request.parentVersionId = "v0";
            request.manifest = makeManifest("song.flp", juce::String::repeatedString("a", 64), {});
            expect(client.createVersionFromManifest("b1", request, "secret-token").ok(), "the version is created");

            request.message.clear();
            expect(client.createVersionFromManifest("b1", request, "secret-token").ok(), "and one without a message");

            const auto requests = server.getRequests();
            expect(requests.size() == 2, "two requests");
            if (requests.size() == 2)
            {
                expect(requests[0].requestLine.startsWith("POST /branches/b1/versions/from-manifest"), requests[0].requestLine);
                const auto withMessage = juce::JSON::parse(requests[0].body);
                expect(withMessage.getProperty("message", {}).toString() == "Drums", "the message goes as \"message\": " + requests[0].body);
                expect(!withMessage.hasProperty("commit_message"), "not as commit_message");
                expect(withMessage.getProperty("parent_version_id", {}).toString() == "v0");
                expect(static_cast<int>(withMessage.getProperty("manifest", {}).getProperty("manifest_version", 0)) == 2,
                       "with the manifest as written");

                const auto withoutMessage = juce::JSON::parse(requests[1].body);
                expect(withoutMessage.isObject() && !withoutMessage.hasProperty("message") && !withoutMessage.hasProperty("commit_message"),
                       "no message, no placeholder: " + requests[1].body);
            }
        }

        beginTest("A version without a file list is reported as such, not as missing");
        {
            LocalHttpServer server([](const LocalHttpServer::Request& request)
            {
                juce::ignoreUnused(request);
                const juce::String body = R"({"id": "v1", "branch_id": "b1"})";
                return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                     + juce::String(static_cast<int>(body.getNumBytesAsUTF8())) + "\r\nConnection: close\r\n\r\n" + body;
            });

            ApiClient client(server.getBaseUrl());
            const auto manifest = client.fetchVersionManifest("v1", "secret-token");
            expect(!manifest.ok() && manifest.error->kind == ApiError::Kind::invalidResponse,
                   "a version the server has, without a manifest, is not a missing version");
            expect(manifest.errorMessage({}).contains("file list"), manifest.errorMessage({}));
        }
    }
};

ApiTests apiTests;
}
