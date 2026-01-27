#include "fileSystem.h"
#include "fileFunctions.h"

namespace lsp {

SourceFile *LocalFileSystem::getFile(const std::string &path) {
	// Check cache first
	auto it = cache.find(path);
	if (it != cache.end()) {
		return it->second.get();
	}

	// Read from disk
	std::string content;
	if (!readStringFromFile(path, content)) {
		return nullptr;
	}

	// Cache and return
	auto [inserted, _] = cache.emplace(path, std::make_unique<SourceFile>(path, std::move(content)));
	return inserted->second.get();
}

} // namespace lsp
