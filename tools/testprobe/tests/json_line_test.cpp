#include "../src/json_line.h"

#include <cassert>
#include <limits>
#include <locale>
#include <string>

namespace {
class DecimalComma final : public std::numpunct<char> {
	char do_decimal_point() const override { return ','; }
};
}

int main()
{
	using namespace skycrpg::testprobe;
	assert(Quote("Iron \"Sword\"\\name") == "\"Iron \\\"Sword\\\"\\\\name\"");
	assert(Quote("\b\f\n\r\t") == "\"\\b\\f\\n\\r\\t\"");
	assert(Quote(std::string("\0\x01\x1f", 3)) == "\"\\u0000\\u0001\\u001f\"");
	assert(Quote("épée 雪 🗡") == "\"épée 雪 🗡\"");
	for (unsigned byte = 0; byte < 32; ++byte) {
		const std::string input(1, static_cast<char>(byte));
		const auto quoted = Quote(input);
		assert(quoted.front() == '"' && quoted.back() == '"');
		for (const char character : quoted) assert(static_cast<unsigned char>(character) >= 32);
	}
	std::locale::global(std::locale(std::locale::classic(), new DecimalComma));
	assert(Number(12.5) == "12.5");
	assert(Number(-42) == "-42");
	assert(Number(std::numeric_limits<std::int64_t>::min()) == "-9223372036854775808");
	assert(Number(std::numeric_limits<std::uint64_t>::max()) == "18446744073709551615");
	assert(Number(std::numeric_limits<double>::infinity()) == "null");
	assert(Number(std::numeric_limits<double>::quiet_NaN()) == "null");
	assert(Number(0.0) == "0");
	assert(FormID(0x123) == "0x00000123");
	assert(FormID(0xFE001ABC) == "0xFE001ABC");
	assert(FormID(0) == "0x00000000");
	assert(FormID(0xFFFFFFFFU) == "0xFFFFFFFF");
	const Object fields = Object{}.String("name", "Sword\n").Boolean("opening", false)
		.Numeric("count_delta", -2).Null("new_count");
	assert(EventLine(1234, "2026-10-28T00:00:00Z", "container", fields) ==
		"{\"t\":1234,\"wall\":\"2026-10-28T00:00:00Z\",\"ev\":\"container\",\"name\":\"Sword\\n\",\"opening\":false,\"count_delta\":-2,\"new_count\":null}\n");
	const Object magic = Object{}.String("selected_name", "Dragonrend").Numeric("selected_filter_flag", 1).Boolean("selected_favorite", true);
	assert(Object{}.Raw("magic", magic.Json()).Json() == "{\"magic\":{\"selected_name\":\"Dragonrend\",\"selected_filter_flag\":1,\"selected_favorite\":true}}");
	assert(Object{}.Null("magic").Json() == "{\"magic\":null}");
	assert(Object{}.Null("selected_name").Null("selected_filter_flag").Null("selected_favorite").Json() == "{\"selected_name\":null,\"selected_filter_flag\":null,\"selected_favorite\":null}");
	assert(EventLine(0, "", "data_loaded") == "{\"t\":0,\"wall\":\"\",\"ev\":\"data_loaded\"}\n");
	assert(ParseRequest("").value().empty());
	const std::string tag = "  checkpoint \"雪\"\\\r\n";
	assert(ParseRequest(tag).value() == tag);
	const std::string binaryTag("a\0b", 3);
	assert(ParseRequest(binaryTag).value() == binaryTag);
	assert(Quote(ParseRequest(binaryTag).value()) == "\"a\\u0000b\"");
	assert(ParseRequest("{\"tag\":\"literal\"}").value() == "{\"tag\":\"literal\"}");
	assert(ParseRequest(std::string(kMaxRequestBytes, 'x')).has_value());
	assert(!ParseRequest(std::string(kMaxRequestBytes + 1, 'x')).has_value());
	assert(ParseInterval("0") == 0);
	assert(ParseInterval("1500") == 1500);
	assert(ParseInterval("2147483647") == 2147483647U);
	for (const auto invalid : { "", "-1", "1x", " 2", "4294967296", "2147483648" }) {
		assert(ParseInterval(invalid) == 1000);
	}
}
