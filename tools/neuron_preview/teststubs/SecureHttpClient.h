#pragma once
// Host test stand-in: serves canned responses registered by URL substring.
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace freeink {

struct CannedResponse {
  std::string urlContains;
  int status;
  std::string body;
};

inline std::vector<CannedResponse>& cannedResponses() {
  static std::vector<CannedResponse> v;
  return v;
}
inline std::vector<std::string>& requestedUrls() {
  static std::vector<std::string> v;
  return v;
}

class SecureHttpClient {
 public:
  using DataCallback = std::function<bool(const uint8_t*, size_t)>;
  using AbortCallback = std::function<bool()>;
  void setInsecure() {}
  void setTimeout(uint32_t) {}
  void setUserAgent(const std::string&) {}
  void setFollowRedirects(int) {}
  bool begin(const std::string& url) {
    url_ = url;
    return url.rfind("https://", 0) == 0;
  }
  void addHeader(const std::string&, const std::string&) {}
  void end() {}
  bool responseComplete() const { return complete_; }
  int GET(const DataCallback& onData, const AbortCallback& = nullptr) {
    requestedUrls().push_back(url_);
    complete_ = false;
    for (const auto& r : cannedResponses()) {
      if (url_.find(r.urlContains) == std::string::npos) continue;
      // Deliver in small chunks, like a real TLS stream.
      for (size_t o = 0; o < r.body.size(); o += 700) {
        const size_t n = std::min<size_t>(700, r.body.size() - o);
        if (!onData(reinterpret_cast<const uint8_t*>(r.body.data() + o), n)) return r.status;
      }
      complete_ = true;
      return r.status;
    }
    return -1;
  }

 private:
  std::string url_;
  bool complete_ = false;
};

}  // namespace freeink
