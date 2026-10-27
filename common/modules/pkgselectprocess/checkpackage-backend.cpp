#include <iostream>
#include <string>
#include <unordered_set>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/cacheiterators.h>
#include <apt-pkg/init.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/progress.h>

// Small helper binary that filters a list of package names against the
// current APT cache. It only outputs packages that actually exist in
// the cache (or tasks/wildcards which are passed through unchanged).
int main(int argc, char* argv[]) {
    pkgInitConfig(*_config);
    pkgInitSystem(*_config, _system);
    if (_system == 0) {
        std::cerr << "apt-pkg not initialized\n";
        return 1;
    }

    // Open the package cache.
    pkgCacheFile *cache = new pkgCacheFile();
    OpProgress progress;
    if (!cache || cache->Open(&progress, false) == false) {
        std::cerr << "Error: could not open APT cache.\n";
        return 1;
    }
    pkgApplyStatus(*cache);

    std::vector<std::string> package_names(argv + 1, argv + argc);
    if (package_names.empty()) return 0;

    std::unordered_set<std::string> seen_packages;
    for (std::string package_name : package_names) {
        if (seen_packages.contains(package_name)) continue;
        seen_packages.insert(package_name);

        // Tasks and wildcards should just be passed through as-is, for now
        if (package_name.starts_with('^') || package_name.contains('*')) {
            std::cout << package_name << " ";
            continue;
        }
        pkgCache::GrpIterator grp = cache->GetPkgCache()->FindGrp(package_name);
        if (!grp.end()) {
            pkgCache::PkgIterator it = grp.FindPreferredPkg(true);
            if (!it.end() && !it.VersionList().end()) {
                std::cout << package_name << " ";
            }
        }
    }

    std::cout << "\n";
    cache->Close();
    return 0;
}
