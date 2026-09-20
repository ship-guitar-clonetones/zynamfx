#include "MockSession.h"

#include <juce_events/juce_events.h>

namespace t3k::ui::testbed {

MockSession::MockSession(const juce::var& scenario, const juce::var& fixtures)
    : authenticated_(static_cast<bool>(scenario.getProperty("auth", false))),
      offline_(static_cast<bool>(scenario.getProperty("offlineAfterLoad", false))),
      gatedPage_(fixtures["gatedPage"]),
      user_(fixtures["user"]),
      apiTones_(fixtures["apiTones"]),
      api_(scenario["api"]) {
  const auto query = scenario["query"].toString();
  if (query.contains("t3k-nav-error"))
    flow_ = {AuthFlow::Phase::error, "Could not reach TONE3000. Check your internet connection and try again."};
  else if (query.contains("code="))
    flow_ = {AuthFlow::Phase::returning, {}};
  // The suite's browserLanding: a canceled browse-intent return opens the
  // in-plugin browser on arrival. Posted: the root subscribes after we exist.
  const auto intent = scenario["sessionStorage"]["t3k.loginIntent"].toString();
  if (query.contains("canceled=true") && intent == "browse")
    juce::MessageManager::callAsync([self = juce::WeakReference<MockSession>(this)] {
      if (self != nullptr && self->onAuthenticated) self->onAuthenticated();
    });
}

MockSession::~MockSession() { masterReference.clear(); }

void MockSession::setFlow(AuthFlow::Phase phase, juce::String error) {
  flow_ = {phase, std::move(error)};
  notifyAuthFlowChanged();
}

void MockSession::clearAuthError() { setFlow(AuthFlow::Phase::idle); }

std::optional<User> MockSession::user() const {
  if (!authenticated_ || !user_.isObject()) return std::nullopt;
  return User::parse(user_);
}

juce::var MockSession::apiTone(int toneId) const {
  const auto* tones = apiTones_.getArray();
  if (tones == nullptr || tones->isEmpty()) return {};
  for (const auto& t : *tones)
    if (static_cast<int>(t["id"]) == toneId) return t;
  return tones->getFirst();
}

juce::var MockSession::override(const char* group) const {
  return api_.isObject() ? api_[group] : juce::var();
}

template <typename T>
void MockSession::answer(const char* group, Reply<T> reply, std::function<Result<T>()> fallback) {
  const auto spec = override(group);
  if (spec.isString() && spec.toString() == "stall") return;  // never replies
  auto result = spec.isString() && spec.toString() == "error"
                    ? Result<T>::fail("Request failed (500).")
                    : fallback();
  juce::MessageManager::callAsync([cb = std::move(reply), r = std::move(result)]() mutable {
    cb(std::move(r));
  });
}

void MockSession::getTone(int toneId, Reply<Tone> reply) {
  answer<Tone>("tone", std::move(reply), [this, toneId] {
    const auto spec = override("tone");
    return Result<Tone>::ok(Tone::parse(spec.isObject() ? spec : apiTone(toneId)));
  });
}

void MockSession::listToneModels(int toneId, const juce::String&, Reply<std::vector<Model>> reply) {
  answer<std::vector<Model>>("models", std::move(reply), [this, toneId] {
    std::vector<Model> models;
    const auto spec = override("models");
    if (spec.isObject()) {
      if (const auto* rows = spec["data"].getArray())
        for (const auto& m : *rows) models.push_back(Model::parse(m));
      return Result<std::vector<Model>>::ok(std::move(models));
    }
    // Pad the picker with the active tone's real sibling models, like the
    // suite's /models handler.
    const auto tone = apiTone(toneId);
    const auto* own = tone["models"].getArray();
    if (own == nullptr || own->isEmpty()) return Result<std::vector<Model>>::ok(std::move(models));
    for (const auto& m : *own) models.push_back(Model::parse(m));
    const auto base = own->getFirst();
    const char* extra[] = {"AC30-6 TB Brl 2", "AC30-6 TB Brl 4", "AC30-6 TB Nrm 2", "AC30-6 TB Nrm 3"};
    int i = 1;
    for (const auto* name : extra) {
      auto copy = juce::var(base.getDynamicObject()->clone().release());
      copy.getDynamicObject()->setProperty("id", static_cast<int>(base["id"]) + i++);
      copy.getDynamicObject()->setProperty("name", name);
      models.push_back(Model::parse(copy));
    }
    return Result<std::vector<Model>>::ok(std::move(models));
  });
}

std::vector<Tone> MockSession::tonesOf(const juce::var& rows) const {
  std::vector<Tone> tones;
  if (const auto* arr = rows.getArray())
    for (const auto& t : *arr) tones.push_back(Tone::parse(t));
  return tones;
}

void MockSession::listTrending(const juce::String&, Reply<std::vector<Tone>> reply) {
  answer<std::vector<Tone>>("trending", std::move(reply), [this] {
    const auto spec = override("trending");
    return Result<std::vector<Tone>>::ok(tonesOf(spec.isObject() ? spec["data"] : apiTones_));
  });
}

void MockSession::listStream(Stream, int, int, const juce::String&, Reply<TonePage> reply) {
  answer<TonePage>("gated", std::move(reply), [this] {
    const auto spec = override("gated");
    const auto& payload = spec.isObject() ? spec : gatedPage_;
    TonePage page;
    page.data = tonesOf(payload["data"]);
    page.page = static_cast<int>(payload.getProperty("page", 1));
    page.totalPages = static_cast<int>(payload.getProperty("total_pages", 1));
    return Result<TonePage>::ok(std::move(page));
  });
}

void MockSession::selectTone(int toneId, Done done) {
  getTone(toneId, [self = juce::WeakReference<MockSession>(this), fin = std::move(done)](Result<Tone> tone) {
    if (self == nullptr) return;
    if (!tone) return fin(tone.error);
    if (self->onToneSelected) self->onToneSelected(*tone);
    fin({});
  });
}

void MockSession::setToneFavorite(int, bool, Done done) {
  juce::MessageManager::callAsync([cb = std::move(done)] { cb({}); });
}

void MockSession::ensureNativeAuth(Done done) {
  juce::MessageManager::callAsync([cb = std::move(done)] { cb({}); });
}

void MockSession::login(LoginIntent intent) {
  const auto authorize = override("authorize");
  if (authorize.isString() && authorize.toString() == "stall") {
    setFlow(AuthFlow::Phase::leaving);  // the browser never comes back
    return;
  }
  setFlow(AuthFlow::Phase::idle);
  authenticated_ = true;
  notifySessionChanged();
  if (intent == LoginIntent::browse && onAuthenticated) onAuthenticated();
}

void MockSession::cancelFlow() {
  if (flow_.phase == AuthFlow::Phase::leaving || flow_.phase == AuthFlow::Phase::returning)
    setFlow(AuthFlow::Phase::idle);
}

void MockSession::logout() {
  authenticated_ = false;
  notifySessionChanged();
}

void MockSession::probeSecureConnection(std::function<void(Probe)> reply) {
  juce::MessageManager::callAsync([cb = std::move(reply)] { cb(Probe::inconclusive); });
}

void MockSession::fetchPluginVersion(Reply<juce::var> reply) {
  // Only scenarios that set `api.version` have a version endpoint at all.
  answer<juce::var>("version", std::move(reply), [this] {
    const auto spec = override("version");
    return spec.isObject() ? Result<juce::var>::ok(spec) : Result<juce::var>::fail("Not found (404).");
  });
}

}  // namespace t3k::ui::testbed
