#pragma once

#include <string_view>

namespace skycrpg::ui {

inline constexpr wchar_t kPinnedAddressLibraryFile[] = L"versionlib-1-7-104-0.bin";
inline constexpr std::string_view kPinnedAddressLibrarySha256 =
	"8aab3dd251d135b849bd983f86a4a205c920fa3e81f8e30c0e63ccfef9423842";

inline bool IsPinnedAddressLibrarySha256(std::string_view digest)
{
	return digest == kPinnedAddressLibrarySha256;
}

}  // namespace skycrpg::ui
