#ifndef OPENTRADE_RISK_H_
#define OPENTRADE_RISK_H_

#include <tbb/atomic.h>
#include <string>

#include "common.h"

namespace opentrade {

struct Limits {
  double msg_rate = 0;               // per second
  double msg_rate_per_security = 0;  // per security per second
  double order_qty = 0;
  double order_value = 0;
  double value = 0;  // per security
  double turnover = 0;
  double total_value = 0;
  double total_turnover = 0;
};

struct Throttle {
  tbb::atomic<int> n = 0;
  int operator()(int tm) const {
    if (tm != this->tm) return 0;
    return n;
  }
  int tm = 0;

  void Update(int tm2) {
    if (tm2 != tm) {
      n = 0;
      tm2 = tm;
    } else {
      n++;
    }
  }
};

inline thread_local std::string kRiskError;

struct Order;

class RiskManager : public Singleton<RiskManager> {
 public:
  bool Check(const Order& ord);
  bool CheckMsgRate(const Order& ord);
  void Disable() { disabled_ = true; }

 private:
  bool disabled_ = false;
};

}  // namespace opentrade

#endif  // OPENTRADE_RISK_H_
