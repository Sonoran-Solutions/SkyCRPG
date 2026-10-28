#include "../src/probe_config.h"

#include <cassert>

int main()
{
	using namespace skycrpg::testprobe;
	// Missing INI or key: GetPrivateProfileStringW returns the empty default.
	assert(!ProbeEnabled(""));
	// Explicit opt-in is the only enabling value.
	assert(ProbeEnabled("1"));
	// Explicitly disabled, and malformed values, stay inert.
	for (const auto value : { "0", "2", "01", "yes", "true", "on", "1 ", "?" })
		assert(!ProbeEnabled(value));
}
