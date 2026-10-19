// stuntman - achievement catalog export. See achievement_catalog.h.

#include "achievement_catalog.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/util/xdbf_utils.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>

namespace {

std::string JsonString(std::string_view s) {
  std::string out = "\"";
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);  // UTF-8 passes through
        }
    }
  }
  return out + "\"";
}

}  // namespace

void WriteAchievementCatalog(rex::system::KernelState* kernel_state,
                             const std::filesystem::path& user_data_root) {
  if (!kernel_state || user_data_root.empty()) return;
  const auto achievements = kernel_state->loaded_achievements();
  if (achievements.empty()) return;

  char title[9];
  std::snprintf(title, sizeof(title), "%08X", kernel_state->title_id());
  const auto dir = user_data_root / "achievements";
  const auto icon_dir = dir / "icons" / title;
  std::error_code ec;
  std::filesystem::create_directories(icon_dir, ec);
  if (ec) {
    REXLOG_WARN("Achievement catalog: cannot create {}: {}", icon_dir.string(), ec.message());
    return;
  }

  // Icons: the XDBF image entries are PNG files. They don't change, so only
  // missing ones are written.
  const auto db = kernel_state->title_xdbf();
  for (const auto& a : achievements) {
    if (a.image_id == 0 || !db.is_valid()) continue;
    const auto path = icon_dir / (std::to_string(a.image_id) + ".png");
    if (std::filesystem::exists(path, ec)) continue;
    const auto block = db.GetEntry(rex::system::util::XdbfSection::kImage, a.image_id);
    if (!block) continue;
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char*>(block.buffer), std::streamsize(block.size));
  }

  // Text is in the language the game runs in, so it's rewritten each launch.
  std::string json = "{\n  \"title_id\": \"" + std::string(title) + "\",\n";
  json += "  \"language\": " + rex::cvar::GetFlagByName("user_language") + ",\n";
  json += "  \"achievements\": [\n";
  for (size_t i = 0; i < achievements.size(); ++i) {
    const auto& a = achievements[i];
    json += "    {\"id\": " + std::to_string(a.id);
    json += ", \"label\": " + JsonString(a.label);
    json += ", \"description\": " + JsonString(a.description);
    json += ", \"unachieved_description\": " + JsonString(a.unachieved_description);
    json += ", \"gamerscore\": " + std::to_string(a.gamerscore);
    json += ", \"image_id\": " + std::to_string(a.image_id);
    json += ", \"flags\": " + std::to_string(a.flags) + "}";
    json += i + 1 < achievements.size() ? ",\n" : "\n";
  }
  json += "  ]\n}\n";

  const auto path = dir / (std::string(title) + ".catalog.json");
  const auto tmp = std::filesystem::path(path).concat(".tmp");
  {
    std::ofstream file(tmp, std::ios::binary);
    if (!file) {
      REXLOG_WARN("Achievement catalog: cannot write {}", tmp.string());
      return;
    }
    file << json;
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    REXLOG_WARN("Achievement catalog: cannot replace {}: {}", path.string(), ec.message());
    return;
  }
  REXLOG_INFO("Achievement catalog: {} entries written to {}", achievements.size(), path.string());
}
