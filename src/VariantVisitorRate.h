namespace o2
{
// ALICE O2 Monitoring system
namespace monitoring
{
/// \brief Subtracts boost variants in order to calculate rate
class VariantVisitorRate : public boost::static_visitor<boost::variant<int, std::string, double, uint64_t>>
{
private:
  /// Timestamp difference in milliseconds
  int timestampCount;

public:
  /// Creates variant visitor functor
  /// \param count  timestamp difference in milliseconds
  VariantVisitorRate(int count) : timestampCount(count) {
  }

  /// Overloads operator() to avoid operating on strings
  /// \throws MonitoringInternalException
  double operator()(const std::string&, const std::string&) const {
    throw MonitoringInternalException("DerivedMetrics/VariantRateVisitor", "Cannot operate on string values");
  }

  /// Calculates rate only when two arguments of the same type are passed
  /// \return calculated rate in Hz
  template<typename T>
  double operator()(const T& a, const T& b) const {
    return static_cast<double>((1000*(a - b)) / timestampCount);
  }

  /// If arguments have different type an exception is raised
  /// \throws MonitoringInternalException
  template<typename T, typename U>
  double operator()(const T&, const U&) const {
    throw MonitoringInternalException("DerivedMetrics/VariantRateVisitor", "Cannot operate on different types");
  }
};

} // namespace monitoring
} // namespace o2
