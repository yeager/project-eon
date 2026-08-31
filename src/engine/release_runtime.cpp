#include "engine/release_runtime.hpp"

#include "platform/game_data.hpp"

namespace eon {

bool ReleaseRuntimeCoordinator::acquire(const ResolvedLaunchRequest& launch) {
    reset();
    // A launcher card can produce this object only through exact hash
    // resolution, but make that invariant explicit at the runtime boundary
    // too. A stale or forged DTO may never retain a previous source.
    if (!launch.request.game || !launch.request.platform || !launch.request.release_sha256
        || !launch.request.release_language
        || *launch.request.game != launch.release.game
        || *launch.request.platform != launch.release.platform
        || *launch.request.release_sha256 != launch.release.sha256
        || *launch.request.release_language != launch.release.language) {
        return false;
    }
    try {
        verify_release_archive(launch.release);
    } catch (...) {
        return false;
    }
    active_ = launch;
    return true;
}

void ReleaseRuntimeCoordinator::reset() {
    active_.reset();
}

} // namespace eon
