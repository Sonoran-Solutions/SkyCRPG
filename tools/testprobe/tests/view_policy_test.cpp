// Copyright (c) 2026 devils_17. SPDX-License-Identifier: MIT
#include "../src/view_policy.h"
#include "../src/observation_policy.h"
#include "../src/perf_policy.h"

#include <cassert>
#include <cmath>
#include <string>

using namespace skycrpg::testprobe;

namespace {
bool Near(double a, double b, double tolerance = 1e-9) { return std::fabs(a - b) <= tolerance; }
bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

HitRecord Hit(Treatment treatment, std::optional<double> alpha = std::nullopt)
{
	HitRecord hit;
	hit.ref = 0x0001A2B3;
	hit.base = 0x00012345;
	hit.layer = 1;
	hit.distance = 100;
	hit.treatment = treatment;
	hit.alpha = alpha;
	return hit;
}
RayRecord Ray(std::initializer_list<HitRecord> hits, bool ran = true)
{
	RayRecord ray;
	ray.ran = ran;
	ray.hits.assign(hits.begin(), hits.end());
	return ray;
}

void Grammar()
{
	assert(ParseViewRequest("").command == ViewCommand::none);
	assert(ParseViewRequest("@view").command == ViewCommand::none);  // no space: raw checkpoint, like "@perf"
	assert(ParseViewRequest("raw tag").command == ViewCommand::none);
	const auto ok = ParseViewRequest("@view arrive-0.a_B");
	assert(ok.command == ViewCommand::sample && ok.label == "arrive-0.a_B");
	for (const auto* bad : { "@view ", "@view a b", "@view a\n", "@view a/b", "@view  a" }) {
		const auto request = ParseViewRequest(bad);
		assert(request.command == ViewCommand::invalid);
	}
	assert(ParseViewRequest("@view " + std::string(40, 'x')).command == ViewCommand::sample);
	assert(ParseViewRequest("@view " + std::string(41, 'x')).command == ViewCommand::invalid);
	// The existing namespaces keep their meaning and do not claim @view.
	assert(ParsePerfRequest("@view a").command == PerfCommand::checkpoint);
	const auto state = ParseStateRequest("@view a");
	assert(state.valid && state.tag == "@view a" && state.count == 0);
	assert(ParseViewRequest("@perf start x").command == ViewCommand::none);
	assert(ParseViewRequest("@state x 0x00000001").command == ViewCommand::none);
}

void Settings()
{
	// Absent keys: feature off, nothing periodic, documented defaults.
	const auto absent = ParseViewSettings("", "", "", "", "");
	assert(!absent.enabled && absent.intervalMs == 0 && absent.ringPoints == 8 && absent.ringRadius == 128 && absent.maxHits == 8);
	assert(!absent.mainThreadOnly && !ParseViewSettings("1", "", "", "", "", "2").mainThreadOnly);
	assert(ParseViewSettings("1", "", "", "", "", "1").mainThreadOnly);
	// The interval is ignored unless the feature is explicitly enabled.
	assert(ParseViewSettings("0", "500", "", "", "").intervalMs == 0);
	assert(ParseViewSettings("2", "500", "", "", "").enabled == false);
	const auto on = ParseViewSettings("1", "500", "12", "256", "4");
	assert(on.enabled && on.intervalMs == 500 && on.ringPoints == 12 && on.ringRadius == 256 && on.maxHits == 4);
	assert(ParseViewInterval("0") == 0 && ParseViewInterval("") == 0 && ParseViewInterval("?") == 0);
	assert(ParseViewInterval("1") == 100 && ParseViewInterval("99") == 100 && ParseViewInterval("100") == 100);
	assert(ParseViewInterval("2147483647") == 2147483647U && ParseViewInterval("2147483648") == 0);
	assert(ParseBounded("17", 0, 16, 8) == 8 && ParseBounded("0", 0, 16, 8) == 0 && ParseBounded("1?", 0, 16, 8) == 8);
	assert(ParseViewSettings("1", "", "", "15", "0").ringRadius == 128);
	assert(ParseViewSettings("1", "", "", "", "0").maxHits == 8);
	assert(ParseViewSettings("1", "", "", "", "17").maxHits == 8);
}

void Projection_()
{
	// Camera at the origin looking down +Y: w = y, x_clip = x / y, y_clip = z / y.
	const float m[4][4] = { { 1, 0, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 0 }, { 0, 1, 0, 0 } };
	const Port port{ 0, 1, 1, 0 };  // the world camera's usual NiRect
	auto p = Project(m, port, { 0, 10, 0 });
	assert(p.valid && p.inFront && p.onScreen && Near(p.x, .5) && Near(p.y, .5) && Near(p.depth, 10));
	p = Project(m, port, { 10, 10, 10 });  // right edge, top edge (y down)
	assert(p.onScreen && Near(p.x, 1) && Near(p.y, 0));
	p = Project(m, port, { -10, 10, -10 });  // left edge, bottom edge
	assert(p.onScreen && Near(p.x, 0) && Near(p.y, 1));
	p = Project(m, port, { 11, 10, 0 });
	assert(p.inFront && !p.onScreen && p.x > 1);
	// Behind the camera: the engine's |w| would mirror (0,-10,0) to screen centre.
	p = Project(m, port, { 0, -10, 0 });
	assert(p.valid && !p.inFront && !p.onScreen);
	p = Project(m, port, { 0, 0, 0 });  // on the camera plane
	assert(p.valid && !p.inFront);
	// A non-unit port maps to the same normalized position.
	p = Project(m, Port{ -1, 1, 1, -1 }, { 5, 10, 5 });
	assert(Near(p.x, .75) && Near(p.y, .25));
	// Engine arithmetic reproduced for a translated, scaled camera.
	const float t[4][4] = { { 2, 0, 0, -4 }, { 0, 0, 2, -6 }, { 0, 0, 0, 0 }, { 0, 1, 0, -100 } };
	p = Project(t, port, { 2, 150, 3 });  // w = 50, x_clip = 0, y_clip = 0
	assert(Near(p.x, .5) && Near(p.y, .5) && Near(p.depth, 50));
	const float bad[4][4] = { { NAN, 0, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 0 }, { 0, 1, 0, 0 } };
	assert(!Project(bad, port, { 0, 10, 0 }).valid);
	assert(!Project(m, Port{ 0, 0, 1, 0 }, { 0, 10, 0 }).valid);
	assert(!Project(m, port, { 0, NAN, 0 }).valid);
}

void Pose()
{
	auto a = ForwardAngles({ 0, 1, 0 });
	assert(a && Near(a->yaw, 0) && Near(a->pitch, 0));
	a = ForwardAngles({ 1, 0, 0 });
	assert(a && Near(a->yaw, 90));
	a = ForwardAngles({ -1, 0, 0 });
	assert(a && Near(a->yaw, 270));
	a = ForwardAngles({ 0, -1, 0 });
	assert(a && Near(a->yaw, 180));
	// 55 degrees down toward the north-east.
	const double down = 55 * kPi / 180;
	a = ForwardAngles({ std::cos(down) * std::sqrt(.5), std::cos(down) * std::sqrt(.5), -std::sin(down) });
	assert(a && Near(a->yaw, 45, 1e-9) && Near(a->pitch, 55, 1e-9));
	assert(!ForwardAngles({ 0, 0, 0 }));
	assert(Near(NormalizeDegrees(-90), 270) && Near(NormalizeDegrees(720), 0));
	assert(Near(*AngleBetweenDegrees({ 1, 0, 0 }, { 0, 1, 0 }), 90));
	assert(Near(*AngleBetweenDegrees({ 1, 0, 0 }, { 2, 0, 0 }), 0));
	assert(!AngleBetweenDegrees({ 0, 0, 0 }, { 1, 0, 0 }));
	const double half = std::tan(37.5 * kPi / 180);
	assert(Near(*FrustumDegrees(-half, half), 75, 1e-9));
	assert(!FrustumDegrees(1, -1) && !FrustumDegrees(NAN, 1));
}

void Rays()
{
	const Vec3 center{ 100, 200, 300 };
	auto r = RingPoint(center, 128, 8, 0, 0, kViewPointLift);
	assert(Near(r.x, 100) && Near(r.y, 328) && Near(r.z, 308));
	r = RingPoint(center, 128, 8, 2, 0, 0);  // 90 degrees clockwise: +X
	assert(Near(r.x, 228, 1e-9) && Near(r.y, 200, 1e-9));
	r = RingPoint(center, 128, 4, 1, 90, 0);  // start east, next south
	assert(Near(r.x, 100, 1e-9) && Near(r.y, 72, 1e-9));
	const auto plan = PlanRay({ 0, 0, 0 }, { 0, 102, 0 });
	assert(plan && Near(plan->length, 100) && Near(plan->to.y, 100));
	assert(!PlanRay({ 0, 0, 0 }, { 0, 3, 0 }));
	assert(!PlanRay({ 0, 0, 0 }, { 0, NAN, 0 }));
	const auto next = Advance({ 0, 0, 0 }, { 0, 100, 0 }, .25);
	assert(Near(next.y, 26));  // hit at 25 units, re-cast one unit past it
	assert(Near(Advance({ 0, 0, 0 }, { 0, 0, 0 }, .5).y, 0));
}

void Folding()
{
	assert(FoldRay(Ray({})).status == RayStatus::clear && *FoldRay(Ray({})).transmittance == 1);
	assert(FoldRay(Ray({ Hit(Treatment::hidden), Hit(Treatment::hidden) })).status == RayStatus::clear);
	// Faded never becomes clear, at any alpha, alone or behind hidden geometry.
	for (const double alpha : { 0.0, 0.01, 0.35, 0.99 }) {
		const auto fold = FoldRay(Ray({ Hit(Treatment::hidden), Hit(Treatment::faded, alpha) }));
		assert(fold.status == RayStatus::faded_only && Near(*fold.transmittance, 1 - alpha));
	}
	const auto single = FoldRay(Ray({ Hit(Treatment::faded, .35) }));
	assert(single.status == RayStatus::faded_only && Near(*single.transmittance, .65));
	// Eight stacked Phase 5 fades: 0.65^8, about 3.2 percent.
	RayRecord stacked = Ray({});
	for (int i = 0; i < 8; ++i) stacked.hits.push_back(Hit(Treatment::faded, .35));
	const auto eight = FoldRay(stacked);
	assert(eight.status == RayStatus::faded_only && Near(*eight.transmittance, std::pow(.65, 8), 1e-12));
	assert(Near(*eight.transmittance, 0.031864481289062, 1e-12) && *eight.transmittance < .033);
	// Any opaque or unknown hit blocks.
	auto fold = FoldRay(Ray({ Hit(Treatment::faded, .35), Hit(Treatment::opaque) }));
	assert(fold.status == RayStatus::blocked && *fold.transmittance == 0);
	fold = FoldRay(Ray({ Hit(Treatment::hidden), Hit(Treatment::unknown) }));
	assert(fold.status == RayStatus::blocked && !fold.transmittance);
	fold = FoldRay(Ray({ Hit(Treatment::faded) }));  // faded without an alpha is unreadable
	assert(fold.status == RayStatus::blocked && !fold.transmittance);
	fold = FoldRay(Ray({ Hit(Treatment::faded, 1.0) }));
	assert(fold.status == RayStatus::blocked);
	// Truncated, not run and changed worlds are indeterminate, never clear.
	RayRecord truncated = Ray({ Hit(Treatment::hidden) });
	truncated.truncated = true;
	assert(FoldRay(truncated).status == RayStatus::indeterminate && !FoldRay(truncated).transmittance);
	assert(RayReason(truncated) == "truncated");
	assert(FoldRay(Ray({}, false)).status == RayStatus::indeterminate);
	assert(RayReason(Ray({}, false)) == "not_run");
	RayRecord changed = Ray({});
	changed.worldChanged = true;
	assert(FoldRay(changed).status == RayStatus::indeterminate && RayReason(changed) == "world_changed");
	RayRecord budget = Ray({ Hit(Treatment::opaque) }, false);
	budget.reason = "cast_budget";
	assert(FoldRay(budget).status == RayStatus::indeterminate && RayReason(budget) == "cast_budget");
}

void Treatments()
{
	TreatmentAccumulator none;
	assert(none.result().treatment == Treatment::unknown && none.result().reason == "no_geometry");
	TreatmentAccumulator faded;
	faded.part({ false, true, true, .35 });
	faded.part({ false, true, true, .5 });
	auto result = faded.result();
	assert(result.treatment == Treatment::faded && Near(*result.alpha, .5));  // most opaque part
	TreatmentAccumulator mixed;
	mixed.part({ false, true, true, .35 });
	mixed.part({ false, false, true, 1 });  // one part draws without blending
	assert(mixed.result().treatment == Treatment::opaque);
	TreatmentAccumulator full;
	full.part({ false, true, true, 1 });
	assert(full.result().treatment == Treatment::opaque);
	TreatmentAccumulator effect;
	effect.part({ false, true, false, .2 });
	assert(effect.result().treatment == Treatment::unknown && effect.result().reason == "non_lighting_blend");
	TreatmentAccumulator invalid;
	invalid.part({ false, true, true, NAN });
	assert(invalid.result().treatment == Treatment::unknown && invalid.result().reason == "invalid_alpha");
	TreatmentAccumulator culledParts;
	culledParts.part({ true, false, true, 1 });
	assert(culledParts.result().treatment == Treatment::hidden);
	TreatmentAccumulator someCulled;
	someCulled.part({ true, false, true, 1 });
	someCulled.part({ false, true, true, .35 });
	assert(someCulled.result().treatment == Treatment::faded);
	TreatmentAccumulator hidden;
	hidden.part({ false, false, true, 1 });
	hidden.hiddenChain();
	assert(hidden.result().treatment == Treatment::hidden);  // app-culled reference wins
	TreatmentAccumulator capped;
	capped.part({ false, true, true, .35 });
	capped.nodeCap();
	assert(capped.result().treatment == Treatment::unknown && capped.result().reason == "node_cap");
}

void Cadence()
{
	ViewCadence cadence;
	std::uint32_t coalesced = 99;
	assert(cadence.tick(coalesced) && coalesced == 0 && cadence.pending());
	assert(!cadence.tick(coalesced) && !cadence.tick(coalesced) && !cadence.tick(coalesced));
	cadence.done();
	assert(cadence.tick(coalesced) && coalesced == 3);  // three merged ticks reported once
	cadence.done();
	assert(cadence.tick(coalesced) && coalesced == 0);
}

void Json()
{
	PointRecord point;
	point.name = "head";
	point.world = Vec3{ 1, 2, 3.5 };
	Projection projection;
	projection.valid = projection.inFront = projection.onScreen = true;
	projection.x = .25;
	projection.y = .5;
	point.projection = projection;
	point.ray = Ray({ Hit(Treatment::faded, .35) });
	point.ray.casts = 2;
	const auto json = PointJson(point);
	assert(json == "{\"name\":\"head\",\"world\":{\"x\":1,\"y\":2,\"z\":3.5},\"screen\":{\"x\":0.25,\"y\":0.5},"
		"\"on_screen\":true,\"ray\":{\"status\":\"faded_only\",\"transmittance\":0.65,\"hits\":[{\"ref\":\"0x0001A2B3\","
		"\"base\":\"0x00012345\",\"layer\":1,\"distance\":100,\"treatment\":\"faded\",\"alpha\":0.35,\"reason\":null}],"
		"\"truncated\":false,\"reason\":null,\"skipped_player\":0,\"casts\":2}}");
	// Projection unavailable: screen and on_screen are null, never false.
	point.projection.reset();
	assert(Has(PointJson(point), "\"screen\":null,\"on_screen\":null"));
	// Behind the camera: known off screen.
	projection.inFront = projection.onScreen = false;
	point.projection = projection;
	assert(Has(PointJson(point), "\"screen\":null,\"on_screen\":false"));
	// A missing point is indeterminate with its reason.
	PointRecord missing;
	missing.name = "shoulder_l";
	missing.ray.reason = "point_unavailable";
	const auto missingJson = PointJson(missing);
	assert(Has(missingJson, "\"world\":null,\"screen\":null,\"on_screen\":null"));
	assert(Has(missingJson, "\"status\":\"indeterminate\",\"transmittance\":null,\"hits\":[]"));
	assert(Has(missingJson, "\"reason\":\"point_unavailable\""));
	// Hidden and unknown hits keep a null alpha; unknown carries its reason.
	HitRecord unknown = Hit(Treatment::unknown, .2);
	unknown.reason = "node_cap";
	unknown.ref.reset();
	assert(HitJson(unknown) == "{\"ref\":null,\"base\":\"0x00012345\",\"layer\":1,\"distance\":100,"
		"\"treatment\":\"unknown\",\"alpha\":null,\"reason\":\"node_cap\"}");
	std::vector<PointRecord> points(4);
	points[0].ray = Ray({});
	points[1].ray = Ray({ Hit(Treatment::faded, .35) });
	points[2].ray = Ray({ Hit(Treatment::opaque) });
	assert(CountsJson(points) == "{\"clear\":1,\"faded_only\":1,\"blocked\":1,\"indeterminate\":1}");
	assert(BoxJson(points) == "null");
	points[0].world = Vec3{};
	points[0].projection = projection;
	points[0].projection->inFront = true;
	points[1].world = Vec3{};
	points[1].projection = projection;
	points[1].projection->inFront = true;
	points[1].projection->x = .75;
	points[1].projection->y = .1;
	assert(BoxJson(points) == "{\"x_min\":0.25,\"x_max\":0.75,\"y_min\":0.1,\"y_max\":0.5}");
	assert(PointsJson({}) == "[]");
	assert(Object{}.Numeric("a", 1).Append(Object{}).Append(Object{}.Null("b")).Json() == "{\"a\":1,\"b\":null}");
	assert(Object{}.Append(Object{}.Boolean("c", true)).Json() == "{\"c\":true}");
	// Envelope: the event line stays one JSON object per line.
	const auto line = EventLine(5, "2026-11-09T00:00:00Z", "view_sample",
		Object{}.Numeric("view_protocol", kViewProtocol).Numeric("seq", 1).Boolean("requested", true)
			.String("tag", "arrive").Boolean("available", false).String("reason", "game_paused"));
	assert(line == "{\"t\":5,\"wall\":\"2026-11-09T00:00:00Z\",\"ev\":\"view_sample\",\"view_protocol\":1,\"seq\":1,"
		"\"requested\":true,\"tag\":\"arrive\",\"available\":false,\"reason\":\"game_paused\"}\n");
}
}  // namespace

int main()
{
	// Existing protocol numbers are unchanged by the additive view event.
	assert(kObservationProtocol == 2 && kViewProtocol == 1 && kViewRayLayer == 41);
	Grammar();
	Settings();
	Projection_();
	Pose();
	Rays();
	Folding();
	Treatments();
	Cadence();
	Json();
}
