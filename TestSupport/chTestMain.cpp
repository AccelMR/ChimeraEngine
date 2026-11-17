/************************************************************************/
/**
 * @file chTestMain.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Catch's main for every test executable, plus a summary per topic.
 */
/************************************************************************/
#include "chAlgorithm.h"
#include "chMath.h"
#include "chPlatformTypes.h"
#include "chSTDHeaders.h"

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace chEngineSDK;

namespace {

/**
 * Exists so a run tells, besides Catch's total, how each topic did. The topic of a test case
 * is its first tag that starts with an upper case letter (TEST_CASE("...", "[Animation]")).
 */
class TopicSummaryListener : public Catch::TestEventListenerBase
{
 public:
  using TestEventListenerBase::TestEventListenerBase;

  void
  testCaseEnded(const Catch::TestCaseStats& stats) override
  {
    const String& topic = getTopic(stats.testInfo);
    TopicTotals* totals = nullptr;
    for (TopicTotals& candidate : m_topics) {
      if (candidate.name == topic) {
        totals = &candidate;
        break;
      }
    }
    if (!totals) {
      totals = &m_topics.emplace_back(TopicTotals{.name = topic});
    }

    ++totals->testCount;
    if (stats.totals.assertions.failed > 0) {
      ++totals->failedCount;
    }
  }

  void
  testRunEnded(const Catch::TestRunStats& stats) override
  {
    CH_PARAMETER_UNUSED(stats);
    if (m_topics.empty()) {
      return;
    }

    Algorithm::sort(m_topics, [](const TopicTotals& a, const TopicTotals& b) {
      return a.name < b.name;
    });

    SIZE_T nameWidth = 0;
    for (const TopicTotals& totals : m_topics) {
      nameWidth = Math::max(nameWidth, totals.name.size());
    }

    stream << "\nTopics:\n";
    for (const TopicTotals& totals : m_topics) {
      stream << "  [" << totals.name << "]" << String(nameWidth - totals.name.size(), ' ')
             << "  " << totals.testCount << (totals.testCount == 1 ? " test  - " : " tests - ");
      if (totals.failedCount == 0) {
        Catch::Colour colour(Catch::Colour::ResultSuccess);
        stream << "All Passed";
      }
      else {
        Catch::Colour colour(Catch::Colour::ResultError);
        stream << totals.failedCount << " failed";
      }
      stream << "\n";
    }
    stream << "\n";
  }

 private:
  struct TopicTotals
  {
    String name;
    uint32 testCount = 0;
    uint32 failedCount = 0;
  };

  static const String&
  getTopic(const Catch::TestCaseInfo& info)
  {
    static const String kNoTopic = "No topic";
    for (const String& tag : info.tags) {
      if (!tag.empty() && tag[0] >= 'A' && tag[0] <= 'Z') {
        return tag;
      }
    }
    return kNoTopic;
  }

  Vector<TopicTotals> m_topics;
};

} // namespace

CATCH_REGISTER_LISTENER(TopicSummaryListener)
