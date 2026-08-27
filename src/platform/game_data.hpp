#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace eon {

enum class Game { millennium, deuteros };
enum class Platform { dos, amiga, atari_st };

struct ReleaseArchive {
    Game game;
    Platform platform;
    std::string language;
    std::string sha256;
    std::filesystem::path path;
};

// A bounded, read-only scan over a user-selected directory.  The launcher
// advances this while rendering so data verification never replaces its first
// frame with a blocking hash pass.  Recognition remains content-addressed.
class ReleaseScanner {
public:
    explicit ReleaseScanner(const std::filesystem::path& directory);

    // Hash at most max_files candidates. Returns true once the scan is done.
    bool advance(std::size_t max_files = 1);
    [[nodiscard]] bool done() const { return next_candidate_ >= candidates_.size(); }
    [[nodiscard]] std::size_t scanned_count() const { return next_candidate_; }
    [[nodiscard]] std::size_t candidate_count() const { return candidates_.size(); }
    [[nodiscard]] const std::vector<ReleaseArchive>& releases() const { return releases_; }

private:
    std::vector<std::filesystem::path> candidates_;
    std::size_t next_candidate_ = 0;
    std::vector<ReleaseArchive> releases_;
};

[[nodiscard]] std::vector<ReleaseArchive> find_release_archives(
    const std::filesystem::path& directory);
[[nodiscard]] std::string name(Game game);
[[nodiscard]] std::string name(Platform platform);

} // namespace eon
