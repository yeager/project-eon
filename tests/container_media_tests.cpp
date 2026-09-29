#include "platform/game_data.hpp"
#include "data/release_manifest.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void require_rejected(const eon::ReleaseArchive& release, const char* message) {
    try {
        static_cast<void>(eon::VerifiedReleaseMedia::open(release));
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error(message);
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Provide the original split-container media directory");
        const auto releases = eon::find_release_archives(argv[1]);
        std::size_t checked = 0;
        for (const auto& release : releases) {
            if (release.layout != eon::ReleaseMediaLayout::verified_container_set) continue;
            const auto sets = eon::container_set_manifest();
            const auto set = std::find_if(sets.begin(), sets.end(), [&release](const auto& item) {
                return item.content_release_sha256 == release.sha256;
            });
            require(set != sets.end(), "Scanner returned an unknown container set");
            require(release.containers.size() >= 2, "Expected a multi-disk set");
            const auto media = eon::VerifiedReleaseMedia::open(release);
            for (const auto& member : set->members) {
                const auto leaf = media.borrow(member.leaf_sha256);
                require(leaf.has_value() && leaf->size() == member.leaf_size,
                    "Admitted set lost an original disk");
                require(eon::to_hex(eon::sha256(*leaf)) == member.leaf_sha256,
                    "Admitted disk bytes differ from their original identity");
            }
            auto changed = release;
            std::swap(changed.containers[0], changed.containers[1]);
            require_rejected(changed, "Reordered runtime disk binding was accepted");
            changed = release;
            changed.containers.pop_back();
            require_rejected(changed, "Incomplete runtime disk binding was accepted");
            changed = release;
            changed.containers[1] = changed.containers[0];
            require_rejected(changed, "Duplicate runtime disk binding was accepted");
            // Rejected descriptors must not invalidate the original selection.
            static_cast<void>(eon::VerifiedReleaseMedia::open(release));
            ++checked;
        }
        require(checked != 0, "No split-container release found in supplied original media");
        std::cout << "Verified original split-container sets: " << checked << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
