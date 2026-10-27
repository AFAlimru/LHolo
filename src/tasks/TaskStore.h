#pragma once
#include "tasks/TaskTypes.h"
#include <filesystem>
namespace lholo::tasks {
Catalog readCatalog(std::filesystem::path const&);
void writeCatalog(std::filesystem::path const&, Catalog const&);
} // namespace lholo::tasks
