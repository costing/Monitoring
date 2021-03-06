#include "Monitoring/DerivedMetrics.h"
#include "../src/Exceptions/MonitoringInternalException.h"
#include <chrono>
#include <thread>
#include <vector>
#include <iostream>

#define BOOST_TEST_MODULE Test Monitoring DerivedMetrics
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

namespace o2 {
namespace monitoring {
namespace Test {

BOOST_AUTO_TEST_CASE(derivedRateInt)
{
  struct RateResults {
    int value;
    int rate;
  };
  std::vector<RateResults> results = {{10, 0 }, {20, 100}, {30, 100}, {50, 200}, {60, 100}, {65, 50}, {70, 50}, {80, 100}, {90, 100}, {100, 100}};
  o2::monitoring::DerivedMetrics derivedHandler;
  std::string name("metricInt");

  for (auto const result : results) {
    try {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      o2::monitoring::Metric metric(result.value, name);
      o2::monitoring::Metric derived = derivedHandler.rate(metric);
      BOOST_CHECK_EQUAL(derived.getName(), "metricIntRate");
      BOOST_WARN_CLOSE(boost::get<double>(derived.getValue()), result.rate, 1.0);
    } catch(MonitoringInternalException &e) {
      BOOST_TEST(e.what() == std::string("Not enough values"));
    }
  }
}

BOOST_AUTO_TEST_CASE(derivedRateDouble) {
  struct RateResults {
    double value;
    double rate;
  };
  std::vector<RateResults> results = {{1.2, 0 }, {11.2, 100}, {21.2, 100}, {41.2, 200}, {51.2, 100}, {61, 98}};
  o2::monitoring::DerivedMetrics derivedHandler;
  std::string name("metricDouble");

  for (auto const result : results) {
    try {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      o2::monitoring::Metric metric(result.value, name);
      o2::monitoring::Metric derived = derivedHandler.rate(metric);
      BOOST_CHECK_EQUAL(derived.getName(), "metricDoubleRate");
      BOOST_WARN_CLOSE(boost::get<double>(derived.getValue()), result.rate, 1.0);
    } catch(MonitoringInternalException &e) {
      BOOST_TEST(e.what() == std::string("Not enough values"));
    }
  }
}

BOOST_AUTO_TEST_CASE(derivedRateUint64_t) {
  struct RateResults {
    uint64_t value;
    uint64_t rate;
  };
  std::vector<RateResults> results = {{165535, 0 }, {165545, 100}, {165555, 100}, {165575, 200}, {165585, 100}, {165590, 50}};
  o2::monitoring::DerivedMetrics derivedHandler;
  std::string name("metricUint64_t");
  
  for (auto const result : results) {
    try {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      o2::monitoring::Metric metric(result.value, name);
      o2::monitoring::Metric derived = derivedHandler.rate(metric);
      BOOST_CHECK_EQUAL(derived.getName(), "metricUint64_tRate");
      BOOST_WARN_CLOSE(boost::get<double>(derived.getValue()), result.rate, 1.0);
    } catch(MonitoringInternalException &e) {
      BOOST_TEST(e.what() == std::string("Not enough values"));
    }
  }
}

bool exceptionCheck(const MonitoringInternalException &e)
{
  if (e.what() == std::string("Not enough values")) return true;
  if (e.what() == std::string("Division by 0")) return true;
  return false;
}

BOOST_AUTO_TEST_CASE(divisionByZero)
{
  std::string name("test");
  o2::monitoring::DerivedMetrics derivedHandler;
  o2::monitoring::Metric metric(10, name, o2::monitoring::Metric::getCurrentTimestamp());

  derivedHandler.rate(metric);
  BOOST_CHECK_EXCEPTION(derivedHandler.rate(metric), MonitoringInternalException, exceptionCheck);
}

BOOST_AUTO_TEST_CASE(derivedIncrementInt) {
  struct IncrementResults {
    int value;
    int rate;
  };
  std::vector<IncrementResults> results = {{10, 10}, {20, 30}, {30, 60}, {50, 110}, {60, 170}, {65, 235}, {70, 305}, {80, 385}, {90, 475}, {100, 575}};
  o2::monitoring::DerivedMetrics derivedHandler;
  std::string name("metricInt");
  for (auto const result : results) {
    o2::monitoring::Metric metric(result.value, name);
    o2::monitoring::Metric derived = derivedHandler.increment(metric);
    BOOST_CHECK_EQUAL(boost::get<int>(derived.getValue()), result.rate);
  }
}

BOOST_AUTO_TEST_CASE(derivedIncrementUint64_t) {
  struct IncrementResults {
    uint64_t value;
    uint64_t rate;
  };
  std::vector<IncrementResults> results = {{5, 5}, {165535, 165540}, {15, 165555}, {50, 165605}, {165590, 331195}};
  o2::monitoring::DerivedMetrics derivedHandler;
  std::string name("metricUint64_t");
  for (auto const result : results) {
    o2::monitoring::Metric metric(result.value, name);
    o2::monitoring::Metric derived = derivedHandler.increment(metric);
    BOOST_CHECK_EQUAL(boost::get<uint64_t>(derived.getValue()), result.rate);
  }
}

BOOST_AUTO_TEST_CASE(derivedIncrementDouble) {
  struct IncrementResults {
    double value;
    double rate;
  };
  std::vector<IncrementResults> results = {{1.2, 1.2}, {11.2, 12.4}, {21.2, 33.6}, {41.2, 74.8}, {51.2, 126}, {61, 187}};
  o2::monitoring::DerivedMetrics derivedHandler;
  std::string name("metricDouble");
  for (auto const result : results) {
    o2::monitoring::Metric metric(result.value, name);
    o2::monitoring::Metric derived = derivedHandler.increment(metric);
    BOOST_CHECK_CLOSE(boost::get<double>(derived.getValue()), result.rate, 0.01);
  }
}

} // namespace Test
} // namespace monitoring
} // namespace o2
