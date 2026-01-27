#pragma once
#include "sourceFile.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace lsp {

// Abstract file system interface for reading source files.
// This allows the compiler to work with different file sources:
// - Local file system (CLI mode)
// - LSP document store (for open files in editor)
// - Remote file requests (for LSP on remote machines)
class FileSystem {
  public:
	virtual ~FileSystem() = default;

	// Get a source file by path. Returns nullptr if file doesn't exist or can't be read.
	// Files are cached - subsequent calls with the same path return the same SourceFile*.
	virtual SourceFile *getFile(const std::string &path) = 0;
};

// Local file system implementation - reads directly from disk and caches results
class LocalFileSystem : public FileSystem {
  public:
	SourceFile *getFile(const std::string &path) override;

  private:
	std::unordered_map<std::string, std::unique_ptr<SourceFile>> cache;
};

} // namespace lsp
