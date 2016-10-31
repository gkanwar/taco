#include "storage.h"

#include <iostream>
#include <string>

#include "error.h"
#include "util/strings.h"

using namespace std;

namespace taco {
namespace storage {

// class Storage
struct Storage::Content {
  Format             format;
  vector<LevelIndex> index;
  double*            values;
};

Storage::Storage() : content(nullptr) {
}

Storage::Storage(const Format& format) : content(new Content) {
  content->format = format;

  vector<Level> levels = format.getLevels();
  content->index.resize(levels.size());
  for (size_t i=0; i < levels.size(); ++i) {
    content->index[i].ptr = nullptr;
    content->index[i].idx = nullptr;
  }
  content->values = nullptr;
}

void Storage::setLevelIndex(int level, int* ptr, int* idx) {
  content->index[level].ptr = ptr;
  content->index[level].idx = idx;
}

void Storage::setValues(double* values) {
  content->values = values;
}

const Format& Storage::getFormat() const {
  return content->format;
}

const Storage::LevelIndex& Storage::getLevelIndex(int level) const {
  return content->index[level];
}

Storage::LevelIndex& Storage::getLevelIndex(const int level) {
  return content->index[level];
}

const double* Storage::getValues() const {
  return content->values;
}

double*& Storage::getValues() {
  return content->values;
}

Storage::Size Storage::getSize() const {
  Storage::Size size;
  int numLevels = content->index.size();

  size.levelIndices.resize(numLevels);
  size_t prevIdxSize = 1;
  for (size_t i=0; i < content->index.size(); ++i) {
    LevelIndex index = content->index[i];
    switch (content->format.getLevels()[i].getType()) {
      case LevelType::Dense:
        size.levelIndices[i].ptr = 1;
        size.levelIndices[i].idx = 0;
        prevIdxSize *= index.ptr[0];
        break;
      case LevelType::Sparse:
        size.levelIndices[i].ptr = prevIdxSize + 1;
        size.levelIndices[i].idx = index.ptr[prevIdxSize];
        prevIdxSize = index.ptr[prevIdxSize];
        break;
      case LevelType::Fixed:
        not_supported_yet;
        break;
    }
  }
  size.values = prevIdxSize;
  return size;
}

bool Storage::defined() const {
  return content != nullptr;
}

std::ostream& operator<<(std::ostream& os, const Storage& storage) {
  auto format = storage.getFormat();
  auto size = storage.getSize();

  // Print indices
  for (size_t i=0; i < format.getLevels().size(); ++i) {
    auto levelIndex = storage.getLevelIndex(i);
    auto levelSize = size.levelIndices[i];

    os << "L" << to_string(i) << ":" << std::endl;
    if (levelSize.ptr > 0) {
      os << "  ptr: {"
         << util::join(&levelIndex.ptr[0], &levelIndex.ptr[levelSize.ptr])
         << "}" << std::endl;
    }
    if (levelSize.idx > 0) {
      os << "  idx: {"
         << util::join(&levelIndex.idx[0], &levelIndex.idx[levelSize.idx])
         << "}" << std::endl;
    }
  }

  // Print values
  os << "vals:  {"
     << util::join(&storage.getValues()[0], &storage.getValues()[size.values])
     << "}";

  return os;
}

}}