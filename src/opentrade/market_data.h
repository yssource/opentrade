#ifndef OPENTRADE_MARKET_DATA_H_
#define OPENTRADE_MARKET_DATA_H_

#include <tbb/concurrent_unordered_map.h>
#include <any>
#include <map>
#include <shared_mutex>
#include <string>

#include "adapter.h"
#include "security.h"

namespace opentrade {

struct MarketData {
#ifdef BACKTEST
  typedef double Qty;
  typedef double Volume;
#else
  typedef int Qty;
  typedef int64_t Volume;
#endif
  time_t tm = 0;
  struct Trade {
    Qty qty = 0;
    double open = 0;
    double high = 0;
    double low = 0;
    double close = 0;
    double vwap = 0;
    Volume volume = 0;

    bool operator!=(const Trade& b) const {
      return volume != b.volume || close != b.close || high != b.high ||
             low != b.low;
    }
  };

  struct Quote {
    double ask_price = 0;
    double bid_price = 0;
    Qty ask_size = 0;
    Qty bid_size = 0;

    bool operator!=(const Quote& b) const {
      return ask_price != b.ask_price || ask_size != b.ask_size ||
             bid_price != b.bid_price || bid_size != b.bid_size;
    }

    bool operator==(const Quote& b) const { return !(*this != b); }
  };

  static inline const size_t kDepthSize = 5;
  typedef Quote Depth[kDepthSize];

  const Quote& quote() const { return depth[0]; }

  Trade trade;
  Depth depth;

  void SetDerived(std::any value, size_t id) {
    assert(id < 16);
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (!derived) derived = new AnyVec;
    if (derived->size() <= id) derived->resize(id);
    (*derived)[id] = value;
  }

  template <typename T>
  const T* GetDerived(size_t id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (!derived) return {};
    if (id >= derived->size()) return {};
    try {
      return std::any_cast<const T*>(derived->at(id));
    } catch (const std::bad_any_cast& e) {
      return {};
    }
  }

 private:
  typedef std::vector<std::any> AnyVec;
  AnyVec* derived = nullptr;
  static inline std::shared_mutex mutex_;
};

struct DataSrc {
  typedef uint32_t IdType;

  IdType value = 0;
  DataSrc() : value(0) {}
  explicit DataSrc(IdType v) : value(v) {}
  explicit DataSrc(const std::string& src) : value(GetId(src.c_str())) {}
  operator IdType() const { return value; }
  const char* str() const { return GetStr(value); }
  const IdType operator()() { return value; }
  DataSrc& operator=(IdType v) {
    value = v;
    return *this;
  }
  DataSrc& operator=(const std::string& v) {
    value = GetId(v.c_str());
    return *this;
  }

  static constexpr IdType GetId(const char* src) {
    if (!src || !*src) return 0;
    return strlen(src) == 1 ? *src : (GetId(src + 1) << 8) + *src;
  }

  static const char* GetStr(IdType id) {
    static thread_local char str[5];
    auto i = 0u;
    for (i = 0u; i < 4 && id; ++i) {
      str[i] = id & 0xFF;
      id >>= 8;
    }
    str[i] = 0;
    return str;
  }
};

class MarketDataAdapter : public virtual NetworkAdapter {
 public:
  typedef tbb::concurrent_unordered_map<Security::IdType, MarketData>
      MarketDataMap;
  virtual void Subscribe(const Security& sec) noexcept = 0;
  DataSrc::IdType src() const { return src_; }
  void Update(Security::IdType id, const MarketData::Quote& q,
              uint32_t level = 0);
  void Update(Security::IdType id, double price, MarketData::Qty size,
              bool is_bid, uint32_t level = 0);
  void Update(Security::IdType id, double last_price, MarketData::Qty last_qty);
  void Update(Security::IdType id, double last_price, MarketData::Volume volume,
              double open, double high, double low, double vwap);
  void UpdateMidAsLastPrice(Security::IdType id);
  void UpdateAskPrice(Security::IdType id, double v);
  void UpdateAskSize(Security::IdType id, double v);
  void UpdateBidPrice(Security::IdType id, double v);
  void UpdateBidSize(Security::IdType id, double v);
  void UpdateLastPrice(Security::IdType id, double v);
  void UpdateLastSize(Security::IdType id, double v);

 protected:
  MarketDataMap* md_ = nullptr;

 private:
  DataSrc::IdType src_ = 0;
  friend class MarketDataManager;
};

class MarketDataManager : public AdapterManager<MarketDataAdapter>,
                          public Singleton<MarketDataManager> {
 public:
  MarketDataAdapter* Subscribe(const Security& sec, DataSrc::IdType src);
  void Add(MarketDataAdapter* adapter);
  const MarketData& Get(const Security& sec, DataSrc::IdType src = 0);
  // Lite version without subscription
  const MarketData& GetLite(Security::IdType id, DataSrc::IdType src = 0);
  MarketDataAdapter* GetDefault() const { return default_; }

 private:
  MarketDataAdapter* GetRoute(const Security& sec, DataSrc::IdType src);

 private:
  std::map<DataSrc::IdType, MarketDataAdapter::MarketDataMap> md_of_src_;
  MarketDataAdapter* default_;
  std::map<std::pair<DataSrc::IdType, Exchange::IdType>,
           std::vector<MarketDataAdapter*>>
      routes_;
};

}  // namespace opentrade

#endif  // OPENTRADE_MARKET_DATA_H_
