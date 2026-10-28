#include "../src/snapshot_policy.h"
#include "../src/json_line.h"

#include <cassert>

int main()
{
	using namespace skycrpg::testprobe;
	// Exhaust every combination, including overlapping failure conditions.
	for (const bool drain : { false, true }) {
		for (const bool sameGeneration : { false, true }) {
			for (const bool loaded : { false, true }) {
				const auto reason = SnapshotReason(drain, 7, sameGeneration ? 7 : 8, loaded);
				assert(reason == (!drain ? "outside_task_drain" : !sameGeneration ?
					"generation_changed" : !loaded ? "not_loaded" : ""));
			}
		}
	}
	// Pending checkpoint tags survive a world transition and a thread rejection.
	const auto tag = ParseRequest(std::string("start\0\n雪", 10));
	assert(tag);
	for (const auto reason : { SnapshotReason(false, 7, 8, false),
		SnapshotReason(true, 7, 8, true), SnapshotReason(true, 8, 8, false) }) {
		const auto fields = Object{}.Boolean("requested", true).String("tag", *tag)
			.Boolean("available", false).String("reason", reason);
		assert(fields.Json().find("\"requested\":true,\"tag\":\"start\\u0000\\n雪\"") != std::string::npos);
		assert(fields.Json().find(std::string("\"reason\":") + Quote(reason)) != std::string::npos);
	}
}
