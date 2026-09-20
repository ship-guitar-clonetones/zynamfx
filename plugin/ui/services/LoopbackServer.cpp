#include "LoopbackServer.h"

#include <juce_events/juce_events.h>

namespace t3k::ui {

namespace {
constexpr int kAcceptPollMs = 250;
constexpr int kReadTimeoutMs = 3000;

const char* kLandingPage =
    "<!doctype html><html><head><meta charset=\"utf-8\"><title>TONE3000</title>"
    "<style>body{margin:0;background:#000;color:#fff;font:16px Arial,sans-serif;display:flex;"
    "align-items:center;justify-content:center;height:100vh;text-align:center}</style></head>"
    "<body><div><p>You can return to the TONE3000 plugin.</p>"
    "<p style=\"color:#a1a1aa;font-size:14px\">This window can be closed.</p></div>"
    "<script>window.close()</script></body></html>";

juce::String httpResponse(int status, const char* reason, const juce::String& body) {
  juce::String head;
  head << "HTTP/1.1 " << status << ' ' << reason << "\r\n"
       << "Content-Type: text/html; charset=utf-8\r\n"
       << "Content-Length: " << body.getNumBytesAsUTF8() << "\r\n"
       << "Connection: close\r\n\r\n";
  return head + body;
}
}  // namespace

LoopbackServer::LoopbackServer() : juce::Thread("oauth-loopback") {}

LoopbackServer::~LoopbackServer() {
  masterReference.clear();
  stop();
}

bool LoopbackServer::start() {
  stop();
  listener_ = std::make_unique<juce::StreamingSocket>();
  // Port 0: the OS picks a free ephemeral port; read it back.
  if (!listener_->createListener(0, "127.0.0.1")) {
    listener_.reset();
    return false;
  }
  port_ = listener_->getBoundPort();
  startThread();
  return true;
}

void LoopbackServer::stop() {
  signalThreadShouldExit();
  if (listener_) listener_->close();  // unblocks waitForNextConnection
  stopThread(kAcceptPollMs * 4);
  listener_.reset();
  port_ = 0;
}

juce::String LoopbackServer::redirectUri() const { return "http://localhost:" + juce::String(port_) + "/"; }

void LoopbackServer::run() {
  while (!threadShouldExit() && listener_ != nullptr) {
    if (listener_->waitUntilReady(true, kAcceptPollMs) != 1) continue;
    std::unique_ptr<juce::StreamingSocket> client(listener_->waitForNextConnection());
    if (client == nullptr) continue;
    serve(*client);
  }
}

void LoopbackServer::serve(juce::StreamingSocket& client) {
  // Read the request head (the browser sends one small GET).
  juce::MemoryBlock buffer;
  char chunk[1024];
  const auto deadline = juce::Time::getMillisecondCounter() + kReadTimeoutMs;
  while (juce::Time::getMillisecondCounter() < deadline && !threadShouldExit()) {
    if (client.waitUntilReady(true, 100) != 1) continue;
    const int n = client.read(chunk, sizeof(chunk), false);
    if (n <= 0) break;
    buffer.append(chunk, static_cast<size_t>(n));
    if (juce::String::fromUTF8(static_cast<const char*>(buffer.getData()), static_cast<int>(buffer.getSize()))
            .contains("\r\n\r\n"))
      break;
  }
  const auto request = juce::String::fromUTF8(static_cast<const char*>(buffer.getData()), static_cast<int>(buffer.getSize()));
  const auto requestLine = request.upToFirstOccurrenceOf("\r\n", false, false);
  const auto target = requestLine.fromFirstOccurrenceOf(" ", false, false).upToFirstOccurrenceOf(" ", false, false);
  const auto path = target.upToFirstOccurrenceOf("?", false, false);
  if (!requestLine.startsWith("GET ") || path != "/") {
    const auto reply = httpResponse(404, "Not Found", "");
    client.write(reply.toRawUTF8(), static_cast<int>(reply.getNumBytesAsUTF8()));
    return;
  }
  const auto reply = httpResponse(200, "OK", kLandingPage);
  client.write(reply.toRawUTF8(), static_cast<int>(reply.getNumBytesAsUTF8()));
  const auto query = target.fromFirstOccurrenceOf("?", false, false);
  juce::MessageManager::callAsync([self = juce::WeakReference<LoopbackServer>(this), query] {
    if (self != nullptr && self->onCallback) self->onCallback(query);
  });
  signalThreadShouldExit();  // one redirect per flow
}

}  // namespace t3k::ui
