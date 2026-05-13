#include "ujcore/api_backends/SharedStore.h"

namespace ujcore {

SharedStore& SharedStore::Instance() {
    static SharedStore instance;
    return instance;
}

SharedStore::SharedStore()
    : topoSorter_(state_.topoSortState),
      builder_(state_, topoSorter_) {}

}  // namespace ujcore
