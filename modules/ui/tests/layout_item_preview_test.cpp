#include "../src/inventory/item_preview_policy.h"

#include <cassert>
#include <cmath>
#include <optional>

namespace {
using namespace skycrpg::ui::inventory::preview;

bool Near(double a, double b, double tolerance = 1e-3) { return std::fabs(a - b) <= tolerance; }

// SkyUI 6.x MCM defaults for 16:9: X = -(931.225 - 640) / 8, Z = -(250 - 360) / 8, scale 1.5.
Values SkyUiBase()
{
	const float x = static_cast<float>(-(931.225 - 640.0) / 8.0);
	const float z = static_cast<float>(-(250.0 - 360.0) / 8.0);
	return { x, x, z, z, 1.5f, 1.5f };
}

Values Apply(Values current, const Writes& writes)
{
	for (std::size_t slot = 0; slot < kSlotCount; ++slot) {
		if (writes[slot]) {
			current[slot] = *writes[slot];
		}
	}
	return current;
}

bool Empty(const Writes& writes)
{
	for (const auto& write : writes) {
		if (write) {
			return false;
		}
	}
	return true;
}
}  // namespace

int main()
{
	constexpr double k169 = 16.0 / 9.0;

	// Config parsing: empty keys mean "computed", junk is rejected, values bounded.
	const auto defaults = ParseConfig(L"", L"", L"");
	assert(defaults.config.manage && !defaults.config.scaleFactor && !defaults.config.xShift);
	assert(defaults.manageValid && defaults.scaleValid && defaults.shiftValid);
	const auto disabled = ParseConfig(L"0", L"0.8", L"-12.5");
	assert(!disabled.config.manage && *disabled.config.scaleFactor == 0.8f && *disabled.config.xShift == -12.5f);
	const auto clamped = ParseConfig(L"1", L"3", L"-400");
	assert(clamped.scaleClamped && *clamped.config.scaleFactor == kMaxScaleFactor);
	assert(clamped.shiftClamped && *clamped.config.xShift == kMinXShift);
	assert(*ParseConfig(L"1", L"0.1", L"").config.scaleFactor == kMinScaleFactor);
	for (const auto* junk : { L"2", L"yes", L"-1", L"1.0" }) {
		const auto parsed = ParseConfig(junk, L"", L"");
		assert(!parsed.manageValid && parsed.config.manage);
	}
	for (const auto* junk : { L"nan", L"inf", L"0.5x", L"--1", L"1e999", L"123456789012345678901234567890123" }) {
		const auto parsed = ParseConfig(L"1", junk, junk);
		assert(!parsed.scaleValid && !parsed.shiftValid);
		assert(!parsed.config.scaleFactor && !parsed.config.xShift);
	}

	// Target geometry at 16:9: the column starts 0.66 W in and ends 16 from the edge.
	const Column column = ModelColumn(k169);
	assert(Near(column.left, 844.8) && Near(column.right, 1264.0));
	// Shift: column centre 1054.4 vs SkyUI target 931.225 -> -15.397 position units.
	const Config automatic{};
	assert(Near(ComputeXShift(k169, automatic), -15.3969, 1e-3));
	// Scale: 0.9 * 419.2 / (339 * 1.5) = 0.742; the Elder Scroll then spans 377 units.
	const float factor = ComputeScaleFactor(k169, 1.5f, automatic);
	assert(Near(factor, 0.74196, 1e-4));
	assert(kWorstWidthPerScale * 1.5 * factor <= column.right - column.left);
	// The worst model's horizontal extent sits inside the column.
	const double centre = 931.225 - 8.0 * ComputeXShift(k169, automatic);
	const double half = kWorstWidthPerScale * 1.5 * factor / 2.0;
	assert(centre - half >= column.left && centre + half <= column.right);
	// Narrower displays keep the 1280 stage; ultrawide uses SkyUI's wider target.
	assert(Near(ModelColumn(1.6).left, column.left) && Near(ModelColumn(0.0).left, column.left));
	assert(ModelColumn(21.0 / 9.0).right > column.right);
	assert(ComputeXShift(32.0 / 9.0, automatic) >= kMinXShift);
	assert(ComputeScaleFactor(k169, 0.0f, automatic) == kMaxScaleFactor);
	// Overrides replace the computed values.
	const Config manual{ true, 0.6f, -10.0f };
	assert(ComputeXShift(k169, manual) == -10.0f && ComputeScaleFactor(k169, 1.5f, manual) == 0.6f);

	const Values base = SkyUiBase();
	const Values placed = Placed(base, k169, automatic);
	assert(placed[kPosZ] == base[kPosZ] && placed[kPosZWide] == base[kPosZWide]);
	assert(placed[kPosX] < base[kPosX] && placed[kPosXWide] < base[kPosXWide]);
	assert(Near(placed[kScale], 1.5 * factor, 1e-4) && Near(placed[kScaleWide], 1.5 * factor, 1e-4));

	// State machine: open -> close restores exactly.
	{
		PlacementState state;
		Values engine = base;
		engine = Apply(engine, state.Open(engine, k169, automatic));
		assert(state.Applied() && engine == placed);
		engine = Apply(engine, state.Restore(engine));
		assert(!state.Applied() && engine == base);
		// A second close/load is a no-op.
		assert(Empty(state.Restore(engine)));
	}
	// Reopen without a close never captures our own values as a base.
	{
		PlacementState state;
		Values engine = Apply(base, state.Open(base, k169, automatic));
		assert(Empty(state.Open(engine, k169, automatic)));
		engine = Apply(engine, state.Restore(engine));
		assert(engine == base);
	}
	// Load while open (kPreLoadGame) restores; the next open re-reads the base,
	// so an MCM re-apply on game reload is respected.
	{
		PlacementState state;
		Values engine = Apply(base, state.Open(base, k169, automatic));
		engine = Apply(engine, state.Restore(engine));
		assert(engine == base);
		Values mcm = base;
		mcm[kPosX] += 4.0f;
		mcm[kScale] = 1.2f;
		engine = Apply(mcm, state.Open(mcm, k169, automatic));
		assert(engine == Placed(mcm, k169, automatic));
		engine = Apply(engine, state.Restore(engine));
		assert(engine == mcm);
	}
	// An external change while applied wins over our restore for that slot only.
	{
		PlacementState state;
		Values engine = Apply(base, state.Open(base, k169, automatic));
		engine[kScale] = 2.0f;
		engine = Apply(engine, state.Restore(engine));
		assert(engine[kScale] == 2.0f);
		assert(engine[kPosX] == base[kPosX] && engine[kScaleWide] == base[kScaleWide]);
	}
	// Missing or wrong-typed setting: nothing written, logged once, never applied.
	{
		PlacementState state;
		assert(Empty(state.Open(std::nullopt, k169, automatic)));
		assert(!state.Applied());
		assert(state.TakeMissingLog() && !state.TakeMissingLog());
		assert(Empty(state.Open(std::nullopt, k169, automatic)));
		assert(!state.TakeMissingLog());
		assert(Empty(state.Restore(std::nullopt)));
	}
	// Disabled by config: never applies; turning it off while applied restores.
	{
		PlacementState state;
		const Config off{ false, std::nullopt, std::nullopt };
		assert(Empty(state.Open(base, k169, off)) && !state.Applied());
		Values engine = Apply(base, state.Open(base, k169, automatic));
		engine = Apply(engine, state.Open(engine, k169, off));
		assert(!state.Applied() && engine == base);
	}
	return 0;
}
