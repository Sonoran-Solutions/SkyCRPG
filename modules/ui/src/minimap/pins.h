#pragma once

#include <array>
#include <string_view>

namespace skycrpg::ui::minimap
{
struct Pin { std::string_view path; std::string_view sha256; };
inline constexpr std::array<Pin, 5> kPins{{
    {"SKSE/Plugins/DragonsEyeMinimap.dll", "918fc9b3f4bf284fb1ae8d028370ebd20908d2750f4706383fe56012bddc5c1a"},
    {"SKSE/Plugins/InfinityUI.dll", "53ef0932485ee1bb281cc4b2baad1fe91e368b9d656989071747b2f1316a85ed"},
    {"SKSE/Plugins/LocalMapUpgrade.dll", "f0fb1c9c822133f9e4ece866fc4f6190424dcafac35e040cd0b4cd037340b3d5"},
    {"Interface/InfinityUI/HUDMenu/HUDMovieBaseInstance/Minimap.swf", "dabdfeb3454d0a40d347ad56ab3e252578a9607e9d7d873d8e70183ffc1dd4e5"},
    {"Interface/InfinityUI/HUDMenu/HUDMovieBaseInstance/!assets/MinimapArt.swf", "8eebe684e51f914863918cc46ea14dbf572f766ba4c1be3a99747fdc41dd81ef"},
}};
}
