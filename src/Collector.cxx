///
/// \file Collector.cxx
/// \author Adam Wegrzynek <adam.wegrzynek@cern.ch>
///

#include "Monitoring/Collector.h"

#include <boost/lexical_cast.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <Configuration/ConfigurationFactory.h>

#include "MonInfoLogger.h"
#include "InfoLoggerBackend.h"

#ifdef _WITH_APPMON
#include "ApMonBackend.h"
#endif

#ifdef _WITH_INFLUX
#include "InfluxBackend.h"
#endif

namespace AliceO2 
{
/// ALICE O2 Monitoring system
namespace Monitoring 
{
/// Core features of ALICE O2 Monitoring system
namespace Core
{

Collector::Collector(ConfigFile &configFile)
{
  if (configFile.getValue<int>("InfoLoggerBackend.enable") == 1) {
    mBackends.emplace_back(std::unique_ptr<Backend>(new InfoLoggerBackend()));
  }
#ifdef _WITH_APPMON
  if (configFile.getValue<int>("AppMon.enable") == 1) {
    mBackends.emplace_back(std::unique_ptr<Backend>(
      new ApMonBackend(configFile.getValue<string>("AppMon.pathToConfig"))
    ));
  }
#endif

#ifdef _WITH_INFLUX
  if (configFile.getValue<int>("InfluxDB.enable") == 1) {
    std::string url = configFile.getValue<string>("InfluxDB.hostname") + ":" +  configFile.getValue<string>("InfluxDB.port")
                    + "/write?db=" + configFile.getValue<string>("InfluxDB.db");
    mBackends.emplace_back(std::unique_ptr<Backend>(new InfluxBackend(url)));
  }
#endif
  setDefaultEntity();
  
  mProcessMonitor = std::unique_ptr<ProcessMonitor>(new ProcessMonitor());
  if (configFile.getValue<int>("ProcessMonitor.enable") == 1) {
    int interval = configFile.getValue<int>("ProcessMonitor.interval");
    mMonitorRunning = true;
    mMonitorThread = std::thread(&Collector::processMonitorLoop, this, interval);  
    MonInfoLogger::GetInstance() << "Process Monitor : Automatic updates enabled" << AliceO2::InfoLogger::InfoLogger::endm;
  }

  mDerivedHandler = std::unique_ptr<DerivedMetrics>(new DerivedMetrics(configFile.getValue<int>("DerivedMetrics.maxCacheSize")));
}

Collector::~Collector()
{
  mMonitorRunning = false;
  if (mMonitorThread.joinable()) {
    mMonitorThread.join();
  }

}

void Collector::setDefaultEntity()
{
  char hostname[255];
  gethostname(hostname, 255);

  std::ostringstream format;
  format << hostname << "." << getpid();

  mEntity = format.str();
}
void Collector::setEntity(std::string entity) {
	mEntity = entity;
}

void Collector::sendProcessMonitorValues()
{
  ///                         type    name    value
  /// std::tuple<ProcessMonitorType, string, string>
  for (auto const pid : mProcessMonitor->getPidsDetails()) {
    switch (std::get<0>(pid)) {
      case ProcessMonitorType::INT : sendRawValue(boost::lexical_cast<int>(std::get<1>(pid)), std::get<2>(pid));
               break;
      case ProcessMonitorType::DOUBLE : sendRawValue(boost::lexical_cast<double>(std::get<1>(pid)), std::get<2>(pid));
               break;
      case ProcessMonitorType::STRING : sendRawValue(std::get<1>(pid), std::get<2>(pid));
               break;
    }
  }
}

void Collector::processMonitorLoop(int interval)
{
  while (mMonitorRunning) {
    sendProcessMonitorValues();
    std::this_thread::sleep_for (std::chrono::seconds(interval));
  }
}

void Collector::addMonitoredPid(int pid)
{
  mProcessMonitor->addPid(pid);
}

std::chrono::time_point<std::chrono::system_clock> Collector::getCurrentTimestamp()
{
  return std::chrono::system_clock::now();
}

template<typename T>
void Collector::sendMetric(std::unique_ptr<Metric> metric, T)
{
  for (auto& b: mBackends) {
    b->send(boost::get<T>(metric->getValue()), metric->getName(), metric->getEntity(), metric->getTimestamp());
  }
}

void Collector::addDerivedMetric(DerivedMetricMode mode, std::string name) {
  mDerivedHandler->registerMetric(mode, name);
}

template<typename T> 
inline void Collector::sendRawValue(T value, std::string name, std::chrono::time_point<std::chrono::system_clock> timestamp) const
{
  for (auto& b: mBackends) {
    b->send(value, name, mEntity, timestamp);
  }
}

template<typename T>
void Collector::send(T value, std::string name, std::chrono::time_point<std::chrono::system_clock> timestamp)
{
  if (mDerivedHandler->isRegistered(name)) {
    try {
      std::unique_ptr<Metric> derived = mDerivedHandler->processMetric(value, name, mEntity, timestamp);
      if (derived != nullptr) sendMetric(std::move(derived), value);
    } 
    catch (boost::bad_get& e) {
      throw std::runtime_error("Derived metrics failed : metric " + name + " has incorrect type");
    }
  }
  sendRawValue(value, name, timestamp);
}

template void Collector::send(int, std::string, std::chrono::time_point<std::chrono::system_clock>);
template void Collector::send(double, std::string, std::chrono::time_point<std::chrono::system_clock>);
template void Collector::send(std::string, std::string, std::chrono::time_point<std::chrono::system_clock>);
template void Collector::send(uint32_t, std::string, std::chrono::time_point<std::chrono::system_clock>);

template void Collector::sendRawValue(int, std::string, std::chrono::time_point<std::chrono::system_clock>) const;
template void Collector::sendRawValue(double, std::string, std::chrono::time_point<std::chrono::system_clock>) const;
template void Collector::sendRawValue(std::string, std::string, std::chrono::time_point<std::chrono::system_clock>) const;
template void Collector::sendRawValue(uint32_t, std::string, std::chrono::time_point<std::chrono::system_clock>) const;

} // namespace Core
} // namespace Monitoring
} // namespace AliceO2

