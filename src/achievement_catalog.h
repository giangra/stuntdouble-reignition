// stuntman - achievement catalog export
//
// Names, descriptions, gamerscore and icons live in the XEX's XDBF resource,
// which only the runtime can read (the XEX is encrypted). On each launch we
// write them out for the launcher's Achievements tab:
//
//   <user data>/achievements/<TITLEID>.catalog.json
//   <user data>/achievements/icons/<TITLEID>/<image id>.png
//
// The unlock state itself is the runtime's own <TITLEID>.toml next to it.

#pragma once

#include <filesystem>

namespace rex::system {
class KernelState;
}

void WriteAchievementCatalog(rex::system::KernelState* kernel_state,
                             const std::filesystem::path& user_data_root);
