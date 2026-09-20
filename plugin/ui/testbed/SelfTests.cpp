// Unit tests for the UI's pure logic (`UiTestbed --selftest`): the pieces
// pixel comparison can't pin down, such as parsers, wrapping and state
// machines. Each juce::UnitTest here mirrors one core/ or services/ file.
#include "SelfTests.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <iostream>
#include <thread>

#include "core/Pitch.h"
#include "core/RichText.h"
#include "model/Tone.h"
#include "services/ConnectionGate.h"
#include "services/LoopbackServer.h"
#include "services/OAuth.h"
#include "services/Tone3000Client.h"
#include "services/UiPrefs.h"
#include "services/UpdateCheck.h"
#include "views/browser/Paginator.h"

namespace t3k::ui::testbed {

namespace {

juce::String flat(const RichText& runs) {
  juce::String s;
  for (const auto& r : runs) s += (r.bold ? "*" : "") + (r.href.isNotEmpty() ? "[" + r.text + "]" : r.text) + (r.bold ? "*" : "") + "|";
  return s;
}

struct HtmlTests : juce::UnitTest {
  HtmlTests() : juce::UnitTest("Html::toRichText", "ui") {}
  void runTest() override {
    beginTest("formatting subset");
    expectEquals(flat(Html::toRichText("<p>New in <b>1.5</b>: faster.</p>")), juce::String("New in |*1.5*|: faster.|"));
    expectEquals(flat(Html::toRichText("a<br>b")), juce::String("a|\n|b|"));
    expectEquals(flat(Html::toRichText("<ul><li>one</li><li>two</li></ul>")),
                 juce::String::fromUTF8("\xe2\x80\xa2 one|\n|\xe2\x80\xa2 two|"));

    beginTest("links keep http(s) only");
    const auto link = Html::toRichText("<a href=\"https://x.y/z\">site</a> <a href='javascript:evil()'>no</a>");
    expectEquals(link.size(), static_cast<size_t>(2));
    expectEquals(link[0].href, juce::String("https://x.y/z"));
    expect(link[1].href.isEmpty());

    beginTest("unknown tags drop, entities decode");
    expectEquals(flat(Html::toRichText("<script>x</script><span>a &amp; b &lt;c&gt;</span>")),
                 juce::String("xa & b <c>|"));
  }
};

struct RichFlowTests : juce::UnitTest {
  RichFlowTests() : juce::UnitTest("RichFlow", "ui") {}
  void runTest() override {
    beginTest("wraps across runs and keeps style per word");
    RichText runs{TextRun::strong("No audio input."), TextRun::plain(" No input device is selected.")};
    const RichFlow narrow(runs, 13, 18, 120);
    expect(narrow.lineCount() >= 3);
    expect(narrow.maxLineWidth() <= 120);
    const RichFlow wide(runs, 13, 18, 2000);
    expectEquals(wide.lineCount(), 1);

    beginTest("paragraph breaks force lines");
    RichText paras{TextRun::plain("a"), paragraphBreak(), TextRun::plain("b")};
    expectEquals(RichFlow(paras, 13, 18, 500).lineCount(), 2);

    beginTest("link hit-testing");
    RichText linky{TextRun::plain("see "), TextRun::link("here", "https://t.co")};
    const RichFlow flow(linky, 13, 18, 500);
    expectEquals(flow.linkAt({flow.maxLineWidth() - 2, 9}, {0, 0}), juce::String("https://t.co"));
    expect(flow.linkAt({1, 9}, {0, 0}).isEmpty());
  }
};

struct UpdateCheckTests : juce::UnitTest {
  UpdateCheckTests() : juce::UnitTest("UpdateCheck", "ui") {}
  void runTest() override {
    beginTest("compareVersions");
    expect(UpdateCheck::compareVersions("1.5.0", "1.4.9") > 0);
    expect(UpdateCheck::compareVersions("v1.5", "1.5.0") == 0);
    expect(UpdateCheck::compareVersions("1.5.0-beta", "1.5.0") == 0);
    expect(UpdateCheck::compareVersions("1.10", "1.9") > 0);
    expect(UpdateCheck::compareVersions("0.9", "1.0") < 0);

    beginTest("parsePayload rejects bad shapes and non-http urls");
    auto make = [](const char* url) {
      auto* o = new juce::DynamicObject();
      o->setProperty("version", "2.0.0");
      o->setProperty("message_html", "<p>hi</p>");
      o->setProperty("url", url);
      return juce::var(o);
    };
    expect(UpdateCheck::parsePayload(make("https://www.tone3000.com/plugin")).has_value());
    expect(!UpdateCheck::parsePayload(make("javascript:alert(1)")).has_value());
    expect(!UpdateCheck::parsePayload(juce::var("nope")).has_value());
  }
};

// A signed-out session whose reachability the test scripts.
struct ScriptedSession : ToneSession {
  bool isOnline = true;
  std::vector<Probe> probes;  // replies in order; empty = inconclusive
  int probeCalls = 0;

  bool online() const override { return isOnline; }
  void probeSecureConnection(std::function<void(Probe)> reply) override {
    ++probeCalls;
    const auto result = probes.empty() ? Probe::inconclusive : probes.front();
    if (!probes.empty()) probes.erase(probes.begin());
    reply(result);
  }

  bool authenticated() const override { return false; }
  std::optional<User> user() const override { return std::nullopt; }
  void getTone(int, Reply<Tone> reply) override { reply(Result<Tone>::fail("n/a")); }
  void listToneModels(int, const juce::String&, Reply<std::vector<Model>> reply) override {
    reply(Result<std::vector<Model>>::fail("n/a"));
  }
  void setToneFavorite(int, bool, Done done) override { done("n/a"); }
  void listTrending(const juce::String&, Reply<std::vector<Tone>> reply) override {
    reply(Result<std::vector<Tone>>::fail("n/a"));
  }
  void listStream(Stream, int, int, const juce::String&, Reply<TonePage> reply) override {
    reply(Result<TonePage>::fail("n/a"));
  }
  void selectTone(int, Done done) override { done("n/a"); }
  void ensureNativeAuth(Done done) override { done("n/a"); }
  void login(LoginIntent) override {}
  void startSelectFlow() override {}
  void logout() override {}
  const AuthFlow& authFlow() const override { return flow; }
  void retryFlow() override {}
  void cancelFlow() override {}
  void clearAuthError() override {}
  void fetchPluginVersion(Reply<juce::var> reply) override { reply(Result<juce::var>::fail("n/a")); }
  AuthFlow flow;
};

struct ConnectionGateTests : juce::UnitTest {
  ConnectionGateTests() : juce::UnitTest("ConnectionGate", "ui") {}
  void runTest() override {
    beginTest("offline queues the action; retry releases it once online");
    ScriptedSession session;
    session.isOnline = false;
    ConnectionGate gate(session);
    int ran = 0;
    gate.requireConnection([&] { ++ran; });
    expect(gate.problem() == ConnectionGate::Problem::offline);
    expectEquals(ran, 0);
    gate.retry();  // still offline: nothing moves
    expect(gate.problem() == ConnectionGate::Problem::offline);
    session.isOnline = true;
    gate.retry();
    expect(!gate.problem());
    expectEquals(ran, 1);

    beginTest("online runs at once and probes in the background, throttled");
    ScriptedSession live;
    ConnectionGate g2(live);
    g2.requireConnection([&] { ++ran; });
    expectEquals(ran, 2);
    expectEquals(live.probeCalls, 1);
    g2.requireConnection([&] { ++ran; });
    expectEquals(live.probeCalls, 1);  // within the TTL

    beginTest("a single insecure result never alarms; retry forces a probe");
    ScriptedSession flaky;
    flaky.probes = {ToneSession::Probe::insecure, ToneSession::Probe::ok};
    ConnectionGate g3(flaky);
    g3.requireConnection({});
    expect(!g3.problem());  // confirmation pending (2s), no alarm yet
    expectEquals(flaky.probeCalls, 1);

    beginTest("dismiss drops the queued action");
    ScriptedSession off;
    off.isOnline = false;
    ConnectionGate g4(off);
    int never = 0;
    g4.requireConnection([&] { ++never; });
    g4.dismiss();
    off.isOnline = true;
    g4.retry();
    expectEquals(never, 0);
    expect(!g4.problem());
  }
};

struct PitchTests : juce::UnitTest {
  PitchTests() : juce::UnitTest("pitch", "ui") {}
  void runTest() override {
    beginTest("frequency to note");
    const auto a4 = pitch::fromFrequency(440);
    expectEquals(a4.name, juce::String("A"));
    expectEquals(a4.octave, 4);
    expectWithinAbsoluteError(a4.cents, 0.0f, 0.01f);
    const auto sharp = pitch::fromFrequency(277.18f);
    expectEquals(sharp.name, juce::String::fromUTF8("C\xe2\x99\xaf"));
    expectEquals(sharp.octave, 4);
    beginTest("lit bars");
    expectEquals(pitch::litCount(0), 1);
    expectEquals(pitch::litCount(50), pitch::kBarsPerSide);
  }
};

struct OAuthTests : juce::UnitTest {
  OAuthTests() : juce::UnitTest("OAuth", "ui") {}
  void runTest() override {
    beginTest("PKCE: base64url, S256 challenge, fresh randomness");
    // RFC 7636 appendix B.
    expectEquals(oauth::Pkce::challengeFor("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"),
                 juce::String("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"));
    const auto a = oauth::Pkce::generate(), b = oauth::Pkce::generate();
    expect(a.verifier != b.verifier && a.state != b.state);
    expectEquals(a.challenge, oauth::Pkce::challengeFor(a.verifier));
    expect(!a.verifier.containsAnyOf("+/=") && !a.state.containsAnyOf("+/="));
    expectEquals(a.verifier.length(), 43);  // 32 bytes → 43 unpadded chars
    expectEquals(a.state.length(), 22);     // 16 bytes → 22

    beginTest("authorize URL carries PKCE and the extras");
    oauth::Pkce pkce;
    pkce.challenge = "CH";
    pkce.state = "ST";
    juce::StringPairArray extra;
    extra.set("select_tone", "true");
    const juce::URL url(oauth::authorizeUrl("https://www.tone3000.com", "pk_x", "http://127.0.0.1:1234/cb", pkce, extra));
    expectEquals(url.toString(false), juce::String("https://www.tone3000.com/api/v1/oauth/authorize"));
    const auto q = url.getParameterNames(), v = url.getParameterValues();
    auto param = [&](const char* name) { return v[q.indexOf(name)]; };
    expectEquals(param("client_id"), juce::String("pk_x"));
    expectEquals(param("redirect_uri"), juce::String("http://127.0.0.1:1234/cb"));
    expectEquals(param("code_challenge"), juce::String("CH"));
    expectEquals(param("code_challenge_method"), juce::String("S256"));
    expectEquals(param("state"), juce::String("ST"));
    expectEquals(param("select_tone"), juce::String("true"));

    beginTest("callback parsing");
    using K = oauth::Callback::Kind;
    auto parse = [](const char* query) { return oauth::Callback::parse(query, "ST"); };
    expect(parse("?code=abc&state=ST").kind == K::code);
    expectEquals(parse("code=abc&state=ST&tone_id=42").toneId, juce::String("42"));
    expect(parse("?code=abc&state=OTHER").kind == K::error);
    expectEquals(parse("?code=abc&state=OTHER").error, juce::String("state_mismatch"));
    expect(parse("?canceled=true&state=ST").kind == K::canceled);
    expect(parse("?canceled=true&code=abc&state=ST").kind == K::code);  // a pick wins over the flag
    expectEquals(parse("?error=access_denied&state=ST").error, juce::String("access_denied"));
    expectEquals(parse("?state=ST").error, juce::String("missing_code"));
  }
};

struct LoopbackServerTests : juce::UnitTest {
  LoopbackServerTests() : juce::UnitTest("LoopbackServer", "ui") {}
  void runTest() override {
    beginTest("serves one redirect on an ephemeral port and hands over the query");
    LoopbackServer server;
    juce::String query;
    server.onCallback = [&](const juce::String& q) { query = q; };
    expect(server.start());
    expect(server.port() > 0);
    expectEquals(server.redirectUri(), "http://localhost:" + juce::String(server.port()) + "/");

    // The browser's GET, from a worker so the message thread stays free for
    // the callback.
    const juce::URL url("http://127.0.0.1:" + juce::String(server.port()) + "/?code=abc&state=xyz");
    std::atomic<int> status{-1};
    juce::String page;
    std::thread fetcher([&] {
      int code = 0;
      auto stream = url.createInputStream(
          juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs(3000).withStatusCode(&code));
      if (stream) page = stream->readEntireStreamAsString();
      status = code;
    });
    const auto deadline = juce::Time::getMillisecondCounter() + 5000;
    while ((query.isEmpty() || status < 0) && juce::Time::getMillisecondCounter() < deadline)
      juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    fetcher.join();
    expectEquals(status.load(), 200);
    expect(page.contains("return to the TONE3000 plugin"));
    expectEquals(query, juce::String("code=abc&state=xyz"));

    beginTest("one redirect per flow: the listener is gone afterwards");
    const auto deadline2 = juce::Time::getMillisecondCounter() + 2000;
    while (server.running() && juce::Time::getMillisecondCounter() < deadline2)
      juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    expect(!server.running());
    server.stop();
    expectEquals(server.port(), 0);
  }
};

struct PaginatorTests : juce::UnitTest {
  PaginatorTests() : juce::UnitTest("Paginator", "ui") {}
  void runTest() override {
    using V = std::vector<int>;
    beginTest("seven or fewer pages list them all");
    expect(Paginator::pagesFor(1, 1) == V{1});
    expect(Paginator::pagesFor(4, 7) == V{1, 2, 3, 4, 5, 6, 7});
    beginTest("past seven: head, tail and a window (0 = ellipsis)");
    expect(Paginator::pagesFor(1, 20) == V{1, 2, 3, 4, 0, 20});
    expect(Paginator::pagesFor(3, 20) == V{1, 2, 3, 4, 0, 20});
    expect(Paginator::pagesFor(4, 20) == V{1, 0, 3, 4, 5, 0, 20});
    expect(Paginator::pagesFor(17, 20) == V{1, 0, 16, 17, 18, 0, 20});
    expect(Paginator::pagesFor(18, 20) == V{1, 0, 17, 18, 19, 20});
    expect(Paginator::pagesFor(20, 20) == V{1, 0, 17, 18, 19, 20});
  }
};

// A transport that answers from a script, synchronously, recording requests.
struct ScriptedHttp : HttpTransport {
  struct Sent {
    juce::String url, method, authorization, body;
  };
  std::vector<Sent> sent;
  std::function<HttpResponse(const HttpRequest&)> answer;
  void send(HttpRequest request, std::function<void(HttpResponse)> onDone) override {
    sent.push_back({request.url.toString(true), request.method, request.headers["Authorization"], request.body});
    onDone(answer(request));
  }
};

HttpResponse jsonResponse(int status, const juce::String& body) {
  HttpResponse r;
  r.status = status;
  r.body = body;
  return r;
}

struct Tone3000ClientTests : juce::UnitTest {
  Tone3000ClientTests() : juce::UnitTest("Tone3000Client", "ui") {}
  void runTest() override {
    beginTest("token payloads round-trip; expiry is absolute");
    const auto fromResponse = Tokens::fromTokenResponse(
        juce::JSON::parse(R"({"access_token":"A","refresh_token":"R","expires_in":3600})"), 1'000);
    expect(fromResponse.has_value());
    expectEquals(fromResponse->expiresAtMs, static_cast<juce::int64>(3'601'000));
    expect(!Tokens::fromTokenResponse(juce::JSON::parse(R"({"refresh_token":"R"})"), 0).has_value());
    const auto back = Tokens::fromVar(fromResponse->toVar());
    expect(back && back->access == "A" && back->refresh == "R" && back->expiresAtMs == fromResponse->expiresAtMs);

    beginTest("tokens persist in prefs and clear on demand");
    UiPrefs prefs;
    ScriptedHttp http;
    Tone3000Client client(http, prefs, "https://api.test", "pk_test");
    client.now = [] { return juce::int64{1'000'000}; };
    expect(!client.authenticated());
    client.setTokens({"A1", "R1", 1'000'000 + 3'600'000});
    expect(client.authenticated());
    {
      Tone3000Client again(http, prefs, "https://api.test", "pk_test");
      expect(again.authenticated() && again.tokens()->access == "A1");
    }

    beginTest("a fresh token is used as is; a near-expired one refreshes first");
    http.answer = [](const HttpRequest& r) {
      if (r.url.toString(false).endsWith("/oauth/token"))
        return jsonResponse(200, R"({"access_token":"A2","refresh_token":"R2","expires_in":3600})");
      return jsonResponse(200, R"({"id":7})");
    };
    juce::String token;
    client.getAccessToken([&](Result<juce::String> r) { token = r ? *r : juce::String(); });
    expectEquals(token, juce::String("A1"));
    expectEquals(static_cast<int>(http.sent.size()), 0);
    client.setTokens({"A1", "R1", 1'000'000 + Tone3000Client::kRefreshLeadMs / 2});
    int updates = 0;
    client.onTokensUpdated = [&](const Tokens&) { ++updates; };
    client.getAccessToken([&](Result<juce::String> r) { token = r ? *r : juce::String(); });
    expectEquals(token, juce::String("A2"));
    expectEquals(updates, 1);
    expectEquals(static_cast<int>(http.sent.size()), 1);
    expect(http.sent[0].body.contains("grant_type=refresh_token") && http.sent[0].body.contains("refresh_token=R1"));
    expectEquals(client.tokens()->access, juce::String("A2"));

    beginTest("401 refreshes once and retries with the new token");
    http.sent.clear();
    int calls = 0;
    http.answer = [&](const HttpRequest& r) {
      if (r.url.toString(false).endsWith("/oauth/token"))
        return jsonResponse(200, R"({"access_token":"A3","refresh_token":"R3","expires_in":3600})");
      return ++calls == 1 ? jsonResponse(401, "{}") : jsonResponse(200, R"({"id":7})");
    };
    juce::var user;
    client.getUser([&](Result<juce::var> r) { user = r ? *r : juce::var(); });
    expectEquals(static_cast<int>(user["id"]), 7);
    expectEquals(static_cast<int>(http.sent.size()), 3);
    expectEquals(http.sent[0].authorization, juce::String("Bearer A2"));
    expectEquals(http.sent[2].authorization, juce::String("Bearer A3"));

    beginTest("a rejected refresh signs out and reports auth_required");
    bool authRequired = false;
    client.onAuthRequired = [&] { authRequired = true; };
    client.setTokens({"A3", "R3", 0});  // expired
    http.answer = [](const HttpRequest&) { return jsonResponse(400, R"({"error":"invalid_grant"})"); };
    juce::String error;
    client.getAccessToken([&](Result<juce::String> r) { error = r.error; });
    expect(authRequired);
    expect(!client.authenticated());
    expectEquals(error, juce::String("token_refresh_failed"));
    expect(!Tokens::fromVar(prefs.getJson(UiPrefs::kTokens)).has_value());

    beginTest("optional-auth calls fall back to anonymous when the session is gone");
    http.sent.clear();
    http.answer = [](const HttpRequest&) { return jsonResponse(200, "[]"); };
    HttpResponse anon;
    client.fetchOptionalAuth("/tones/trending", {}, [&](Result<HttpResponse> r) { anon = r ? *r : HttpResponse(); });
    expect(anon.ok());
    expectEquals(static_cast<int>(http.sent.size()), 1);
    expect(http.sent[0].authorization.isEmpty());
  }
};

struct ToneModelTests : juce::UnitTest {
  ToneModelTests() : juce::UnitTest("Tone", "ui") {}
  void runTest() override {
    beginTest("withModels patches the parsed list and the raw JSON alike");
    const auto tone = Tone::parse(juce::JSON::parse(
        R"({"id":1,"title":"T","gear":"amp","models_count":2,"a2_models_count":2,"models":[]})"));
    const auto m1 = Model::parse(juce::JSON::parse(R"({"id":10,"name":"Clean","architecture":2})"));
    const auto m2 = Model::parse(juce::JSON::parse(R"({"id":11,"name":"Lead","architecture":2})"));
    const auto patched = tone.withModels({m1, m2});
    expectEquals(static_cast<int>(patched.models.size()), 2);
    expectEquals(patched.models[1].name, juce::String("Lead"));
    const auto round = juce::JSON::parse(patched.toJson());
    expectEquals(round["models"].size(), 2);
    expectEquals(static_cast<int>(round["models"][0]["id"]), 10);
    expectEquals(static_cast<int>(tone.models.size()), 0);  // the source is untouched
  }
};

HtmlTests htmlTests;
RichFlowTests richFlowTests;
UpdateCheckTests updateCheckTests;
ConnectionGateTests connectionGateTests;
PitchTests pitchTests;
OAuthTests oauthTests;
LoopbackServerTests loopbackServerTests;
PaginatorTests paginatorTests;
Tone3000ClientTests tone3000ClientTests;
ToneModelTests toneModelTests;

}  // namespace

int runSelfTests() {
  juce::UnitTestRunner runner;
  runner.setAssertOnFailure(false);
  runner.runTestsInCategory("ui");
  int failures = 0;
  for (int i = 0; i < runner.getNumResults(); ++i) {
    const auto* r = runner.getResult(i);
    failures += r->failures;
    std::cout << r->unitTestName << " / " << r->subcategoryName << ": " << r->passes << " passed, " << r->failures
              << " failed" << std::endl;
    for (const auto& m : r->messages) std::cout << "  " << m << std::endl;
  }
  return failures == 0 ? 0 : 1;
}

}  // namespace t3k::ui::testbed
