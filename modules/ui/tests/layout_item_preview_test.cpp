#include "../src/inventory/item_preview_policy.h"

#include <cassert>
#include <cmath>

namespace {
using namespace skycrpg::ui::inventory::preview;

bool Near(double a, double b, double tolerance = 1e-3) { return std::fabs(a - b) <= tolerance; }

// SkyUI 6.x MCM defaults for 16:9, as cached by the engine at Begin3D.
Placement SkyUiBase() { return { -36.403f, 0.0f, 13.75f, 1.5f }; }

EngineState Rest(const Placement& p) { return { p, p, 0.0f, false }; }

EngineState Write(EngineState state, const Result& result)
{
	if (result.outcome == Outcome::Written) {
		state.live = result.written;
		state.copy = result.written;
	}
	return state;
}
}  // namespace

int main()
{
	constexpr double k169 = 16.0 / 9.0;

	// Config parsing: empty keys mean "computed", junk is rejected, values bounded.
	const auto defaults = ParseConfig(L"", L"", L"");
	assert(defaults.config.manage && !defaults.config.scaleFactor && !defaults.config.xShift);
	const auto disabled = ParseConfig(L"0", L"0.8", L"-12.5");
	assert(!disabled.config.manage && *disabled.config.scaleFactor == 0.8f && *disabled.config.xShift == -12.5f);
	const auto clamped = ParseConfig(L"1", L"3", L"-400");
	assert(clamped.scaleClamped && *clamped.config.scaleFactor == kMaxScaleFactor);
	assert(clamped.shiftClamped && *clamped.config.xShift == kMinXShift);
	for (const auto* junk : { L"2", L"yes", L"-1", L"1.0" }) {
		const auto parsed = ParseConfig(junk, L"", L"");
		assert(!parsed.manageValid && parsed.config.manage);
	}
	for (const auto* junk : { L"nan", L"inf", L"0.5x", L"--1", L"1e999", L"123456789012345678901234567890123" }) {
		const auto parsed = ParseConfig(L"1", junk, junk);
		assert(!parsed.scaleValid && !parsed.shiftValid && !parsed.config.scaleFactor && !parsed.config.xShift);
	}

	// Default target: centre of the right third, 5/6 W = stage 1066.7, column to W - 16.
	const Target fallback = DefaultTarget(k169);
	assert(Near(fallback.centre, 1066.667) && Near(fallback.half, 197.333));
	assert(ValidTarget(fallback) && Near(DefaultTarget(0.0).centre, fallback.centre));
	assert(DefaultTarget(21.0 / 9.0).centre > fallback.centre);
	assert(!ValidTarget({ 600.0, 200.0 }) && !ValidTarget({ 1000.0, 10.0 }) && !ValidTarget({ NAN, 200.0 }));

	// Owner measurement: card centre ~1598 px = stage ~1070, column to 1264.
	const Target card{ 1070.0, 194.0 };
	const Config automatic{};
	const Placement base = SkyUiBase();
	const Placement placed = Placed(base, card, automatic);
	// x = -(1070 - 640) / 8 = -53.75: centre moves from stage 931 to 1070.
	assert(Near(placed.x, -53.75) && placed.y == base.y && placed.z == base.z);
	// scale: 0.9 * 388 / (339 * 1.5) = 0.687 -> the Elder Scroll is 349 units (~522 px).
	const double factor = placed.scale / base.scale;
	assert(Near(factor, 0.9 * 388.0 / 508.5, 1e-4));
	const double half = kWorstWidthPerScale * placed.scale / 2.0;
	assert(card.centre - half >= card.centre - card.half && card.centre + half <= card.centre + card.half);
	// Fallback target gives the ~0.7 factor and ~560 px the owner asked for.
	const Placement fallbackPlaced = Placed(base, fallback, automatic);
	assert(Near(fallbackPlaced.scale / base.scale, 0.6986, 1e-3));
	assert(kWorstWidthPerScale * fallbackPlaced.scale * 1.4945 <= 560.0);
	// Overrides replace the computed values; xShift is relative to the engine base.
	const Placement manual = Placed(base, card, Config{ true, 0.6f, -10.0f });
	assert(Near(manual.x, -46.403) && Near(manual.scale, 0.9));

	// Session: first apply writes; repeat is idempotent.
	{
		Session session;
		session.Begin();
		EngineState engine = Rest(base);
		const auto first = session.Apply(engine, card, automatic);
		assert(first.outcome == Outcome::Written && first.newBase && first.base == base);
		engine = Write(engine, first);
		assert(engine.live == placed && engine.copy == placed);
		for (int i = 0; i < 3; ++i) {
			const auto again = session.Apply(engine, card, automatic);
			assert(again.outcome == Outcome::Unchanged && !again.newBase);
		}
		// A moved card re-targets from the same base, never from our own output.
		const Target moved{ 1100.0, 164.0 };
		const auto retarget = session.Apply(engine, moved, automatic);
		assert(retarget.outcome == Outcome::Written && !retarget.newBase);
		assert(retarget.written == Placed(base, moved, automatic));
		engine = Write(engine, retarget);
		// The copy alone reset (un-zoom to an engine value): rewritten, base unchanged.
		engine.copy = base;
		const auto copyReset = session.Apply(engine, moved, automatic);
		assert(copyReset.outcome == Outcome::Written && !copyReset.newBase && copyReset.base == base);
	}
	// Base change: the engine resets its cache (Begin3D, MCM change) -> new base.
	{
		Session session;
		session.Begin();
		EngineState engine = Write(Rest(base), session.Apply(Rest(base), card, automatic));
		Placement mcm = base;
		mcm.x = -30.0f;
		mcm.scale = 1.2f;
		engine = Rest(mcm);
		const auto reset = session.Apply(engine, card, automatic);
		assert(reset.outcome == Outcome::Written && reset.newBase && reset.base == mcm);
		assert(reset.written == Placed(mcm, card, automatic));
	}
	// Zoom guard: no write while zooming or zoomed; resumes after.
	{
		Session session;
		session.Begin();
		EngineState engine = Rest(base);
		engine.zoomProgress = 0.4f;
		assert(session.Apply(engine, card, automatic).outcome == Outcome::ZoomActive);
		engine.zoomProgress = 0.0f;
		engine.startedZoom = true;
		assert(session.Apply(engine, card, automatic).outcome == Outcome::ZoomActive);
		engine.startedZoom = false;
		engine = Write(engine, session.Apply(engine, card, automatic));
		// Zoomed in: live moved by the engine, copy holds ours; we do not fight it.
		engine.live.y = 40.0f;
		engine.zoomProgress = 1.0f;
		assert(session.Apply(engine, card, automatic).outcome == Outcome::ZoomActive);
		// Zoomed out again: live returns to the copy (ours) -> nothing to do.
		engine.live = engine.copy;
		engine.zoomProgress = 0.0f;
		assert(session.Apply(engine, card, automatic).outcome == Outcome::Unchanged);
	}
	// Closed, disabled, invalid input.
	{
		Session session;
		assert(session.Apply(Rest(base), card, automatic).outcome == Outcome::Invalid);
		session.Begin();
		assert(session.Apply(Rest(base), card, Config{ false, std::nullopt, std::nullopt }).outcome == Outcome::Disabled);
		assert(session.Apply(Rest(base), Target{ 0.0, 0.0 }, automatic).outcome == Outcome::Invalid);
		EngineState bad = Rest(base);
		bad.live.scale = NAN;
		assert(session.Apply(bad, card, automatic).outcome == Outcome::Invalid);
		session.End();
		assert(!session.Open() && session.Apply(Rest(base), card, automatic).outcome == Outcome::Invalid);
		// A new open always starts from the engine's fresh base.
		session.Begin();
		const auto fresh = session.Apply(Rest(base), card, automatic);
		assert(fresh.outcome == Outcome::Written && fresh.newBase);
	}
	return 0;
}
