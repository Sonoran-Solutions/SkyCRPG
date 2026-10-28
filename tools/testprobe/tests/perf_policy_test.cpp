#include "../src/perf_policy.h"
#include <cassert>
#include <limits>
using namespace skycrpg::testprobe;
int main() {
 // Model drains-to-empty: a UI run only reserves main, and main only UI.
 using Hop = FrameChain::Hop;
 using Tick = FrameChain::Tick;
 FrameChain chain;
 assert(!chain.active() && !chain.queued());
 assert(chain.start() && chain.hop() == Hop::ui);
 assert(!chain.start());
 for (int i = 0; i < 100; ++i) {
  assert(chain.uiRun(i * 16.0) == Tick::sample && chain.hop() == Hop::main);
  assert(chain.uiRun(i * 16.0) == Tick::idle); // UI drain cannot sample again
  assert(chain.mainRun() && chain.hop() == Hop::ui);
  assert(!chain.mainRun()); // even a second main drain cannot add a UI hop
 }
 // Stop/restart while either hop is pending reuses its reservation.
 chain.stop(); assert(chain.queued() && !chain.active());
 assert(!chain.start() && chain.hop() == Hop::ui);
 assert(chain.uiRun(2000) == Tick::sample);
 chain.stop(); assert(!chain.start() && chain.hop() == Hop::main);
 assert(chain.mainRun());
 assert(chain.uiRun(2016) == Tick::sample);
 chain.stop(); assert(!chain.mainRun() && !chain.queued());
 assert(chain.start());
 chain.stop(); assert(chain.uiRun(2032) == Tick::idle && !chain.queued());
 assert(chain.start());
 chain.enqueueFailed(); assert(!chain.active() && !chain.queued());
 assert(chain.start());
 for (int i = 0; i < 8; ++i) {
  assert(chain.uiRun(3000 + i * .1) == Tick::sample);
  assert(chain.mainRun());
 }
 assert(chain.uiRun(3001) == Tick::runaway); // ninth within exactly 1 ms
 assert(!chain.active() && !chain.queued());
 assert(chain.uiRun(3001) == Tick::idle); // one abort only
 assert(chain.start()); // guard resets for next capture
 for (int i = 0; i < 30; ++i) {
  assert(chain.uiRun(4000 + i * .2) == Tick::sample); // sliding window, <=6 per ms
  assert(chain.mainRun());
 }
 chain.stop(); assert(!chain.mainRun()); // UI is pending, so helper is idle
 assert(chain.uiRun(5000) == Tick::idle && !chain.queued());
 FrameRing<4> ring;
 assert(ring.summary().count == 0);
 ring.push(10); ring.push(20); ring.push(40); ring.push(60);
 auto s = ring.summary();
 assert(s.count == 4 && s.median == 30 && s.p95 == 60 && s.p99 == 60 && s.maximum == 60 && s.over33 == 2 && s.over50 == 1);
 ring.push(30); ring.push(std::numeric_limits<double>::quiet_NaN());
 s = ring.summary(); assert(s.count == 4 && s.median == 35 && s.maximum == 60);
 ring.clear(); assert(ring.summary().count == 0);
 SettleDetector detector(10); double settled = 0;
 for (int i = 0; i < 5; ++i) assert(!detector.observe(20, i, settled));
 assert(!detector.observe(21, 5, settled));
 for (int i = 0; i < 5; ++i) assert(!detector.observe(19, 6 + i, settled));
 assert(detector.observe(19, 11, settled) && settled == 6);
 SettleDetector missing(0); assert(!missing.observe(1, 1, settled));

 assert(ring.total() == 0);
 assert(ParsePerfRequest("raw\n").command == PerfCommand::checkpoint);
 assert(ParsePerfRequest("@perf start standing").command == PerfCommand::start);
 assert(ParsePerfRequest("@perf stop turn").label == "turn");
 for (const auto text : {"@perf start", "@perf bad x", "@perf start a b", "@perf stop ", "@perf start x\n"})
  assert(ParsePerfRequest(text).command == PerfCommand::invalid);
 InventoryWindow window;
 window.begin(100, 125, 10, true, true);
 for (int i = 0; i < 5; ++i) assert(!window.observe(16, 140 + i * 16));
 assert(!window.settled);
 window.observe(16, 220);
 assert(window.settled && window.settledMs == 40 && window.cold && window.count == 6);
 for (int i = 0; i < 40; ++i) window.observe(16, 240 + i * 16);
 assert(window.count == 30 && window.periods[0] == 16);
 assert(window.observe(16, 3125));
 window.begin(0, 100, 10, false, false);
 for (int i = 0; i < 10; ++i) window.observe(16, 120 + i * 16);
 assert(!window.settled && !window.correlated && !window.cold);
 SettleDetector invalid(std::numeric_limits<double>::quiet_NaN());
 for (int i = 0; i < 10; ++i) assert(!invalid.observe(1, i, settled));
}
