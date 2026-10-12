#include "ToneQuery.h"

namespace t3k::ui {

const char* profileLabel(Profile profile) {
  switch (profile) {
    case Profile::none: return "";
    case Profile::downloaded: return "Recently used";
    case Profile::favorited: return "Favorites";
    case Profile::created: return "Created";
  }
  return "";
}

const char* profileEndpoint(Profile profile) {
  switch (profile) {
    case Profile::none: return "search";
    case Profile::downloaded: return "downloaded";
    case Profile::favorited: return "favorited";
    case Profile::created: return "created";
  }
  return "search";
}

const char* sortLabel(ToneSort sort) {
  switch (sort) {
    case ToneSort::bestMatch: return "Best match";
    case ToneSort::trending: return "Trending";
    case ToneSort::popular: return "Popular";
    case ToneSort::newest: return "Newest";
    case ToneSort::oldest: return "Oldest";
  }
  return "";
}

const char* sortId(ToneSort sort) {
  switch (sort) {
    case ToneSort::bestMatch: return "best-match";
    case ToneSort::trending: return "trending";
    case ToneSort::popular: return "downloads-all-time";
    case ToneSort::newest: return "newest";
    case ToneSort::oldest: return "oldest";
  }
  return "";
}

const std::vector<juce::String>& ToneQuery::picked(Taxonomy kind) const {
  switch (kind) {
    case Taxonomy::tags: return tags;
    case Taxonomy::makes: return makes;
    case Taxonomy::creators: return creators;
  }
  return tags;
}

std::vector<juce::String>& ToneQuery::picked(Taxonomy kind) {
  switch (kind) {
    case Taxonomy::tags: return tags;
    case Taxonomy::makes: return makes;
    case Taxonomy::creators: return creators;
  }
  return tags;
}

namespace {

class QueryString {
public:
  QueryString& add(const char* key, const juce::String& value) {
    if (value.isNotEmpty()) parts_.add(juce::String(key) + "=" + value);
    return *this;
  }
  QueryString& addEscaped(const char* key, const juce::String& value) {
    return add(key, juce::URL::addEscapeChars(value, true));
  }
  QueryString& addFlag(const char* key, bool on) { return add(key, on ? "true" : ""); }
  juce::String str() const { return parts_.isEmpty() ? juce::String() : "?" + parts_.joinIntoString("&"); }

private:
  juce::StringArray parts_;
};

}  // namespace

juce::String ToneQuery::requestPath(int page, int pageSize, int architecture) const {
  // Public Free-tier browsing uses only TONE3000's bounded list endpoints.
  // Full-catalog search and rich filters stay on the hosted Select flow.
  if (profile == Profile::none)
    return gear.isNotEmpty() ? "/api/v1/tones/trending?gear=" + juce::URL::addEscapeChars(gear, true)
                             : "/api/v1/tones/trending";
  // Profile streams are bounded and paginated by the API.
  QueryString qs;
  qs.add("page", juce::String(juce::jmax(1, page)))
      .add("page_size", juce::String(juce::jlimit(1, 100, pageSize)));
  if (gear.isNotEmpty()) qs.addEscaped("gear", gear);
  return "/api/v1/tones/" + juce::String(profileEndpoint(profile)) + qs.str();
}

bool ToneQuery::operator==(const ToneQuery& o) const {
  return text == o.text && sort == o.sort && gear == o.gear && format == o.format && tags == o.tags &&
         makes == o.makes && creators == o.creators && calibrated == o.calibrated && verified == o.verified &&
         profile == o.profile;
}

}  // namespace t3k::ui
