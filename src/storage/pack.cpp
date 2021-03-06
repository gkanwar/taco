#include "taco/storage/pack.h"

#include <climits>

#include "taco/format.h"
#include "taco/error.h"
#include "taco/ir/ir.h"
#include "taco/storage/storage.h"
#include "taco/storage/index.h"
#include "taco/storage/array.h"
#include "taco/storage/array_util.h"
#include "taco/util/collections.h"

using namespace std;

namespace taco {
namespace storage {

#define PACK_NEXT_LEVEL(cend) {                                          \
  if (i + 1 == modeTypes.size()) {                                       \
    if (cbegin < cend) {                                                  \
      memcpy(&values[valuesIndex], &vals[cbegin*dataType.getNumBytes()], dataType.getNumBytes()); \
    }                                                                     \
    else {                                                                 \
      memset(&values[valuesIndex], 0, dataType.getNumBytes());             \
    }                                                                       \
    valuesIndex += dataType.getNumBytes();                                \
  } else {                                                               \
    valuesIndex = packTensor(dimensions, coords, vals, cbegin, (cend), modeTypes, i+1, \
    indices, values, dataType, valuesIndex);                             \
  }                                                                      \
}


/// Count unique entries (assumes the values are sorted)
TypedIndexVector getUniqueEntries(TypedIndexVector v, int startIndex, int endIndex) {
  TypedIndexVector uniqueEntries(v.getType());
  TypedIndexVal prev;
  TypedIndexVal curr;
  if (endIndex - startIndex > 0){
    prev = v[startIndex];
    uniqueEntries.push_back(prev);
    for (int j = startIndex + 1; j < endIndex; j++) {
      curr = v[j];
      taco_iassert(curr >= prev);
      if (curr > prev) {
        prev = curr;
        uniqueEntries.push_back(curr);
      }
    }
  }
  return uniqueEntries;
}


size_t findMaxFixedValue(const vector<int>& dimensions,
                              const vector<TypedIndexVector>& coords,
                              size_t order,
                              const size_t fixedLevel,
                              const size_t i, const size_t numCoords) {
  if (i == order) {
    return numCoords;
  }
  if (i == fixedLevel) {
    auto indexValues = getUniqueEntries(coords[i], 0, coords[i].size());
    return indexValues.size();
  }
  else {
    // Find max occurrences for level i
    size_t maxSize=0;
    DataType coordType = coords[0].getType();
    TypedIndexVector maxCoords(coordType);
    TypedIndexVal coordCur;
    coordCur = coords[i][0];
    size_t sizeCur=1;
    for (size_t j=1; j<numCoords; j++) {
      if (coords[i][j] == coordCur) {
        sizeCur++;
      }
      else {
        if (sizeCur > maxSize) {
          maxSize = sizeCur;
          maxCoords.clear();
          maxCoords.push_back(coordCur);
        }
        else if (sizeCur == maxSize) {
          maxCoords.push_back(coordCur);
        }
        sizeCur=1;
        coordCur = coords[i][j];
      }
    }
    if (sizeCur > maxSize) {
      maxSize = sizeCur;
      maxCoords.clear();
      maxCoords.push_back(coordCur);
    }
    else if (sizeCur == maxSize) {
      maxCoords.push_back(coordCur);
    }

    size_t maxFixedValue=0;
    size_t maxSegment;
    vector<TypedIndexVector> newCoords(order);
    for (size_t i = 0; i < order; i++) {
      newCoords[i] = TypedIndexVector(coordType);
    }
    for (size_t l=0; l<maxCoords.size(); l++) {
      // clean coords for next level
      for (size_t k=0; k<order;k++) {
        newCoords[k].clear();
      }
      for (size_t j=0; j<numCoords; j++) {
        if (coords[i][j] == maxCoords[l]) {
          for (size_t k=0; k<order;k++) {
            newCoords[k].push_back(coords[k][j]);
          }
        }
      }
      maxSegment = findMaxFixedValue(dimensions, newCoords, order, fixedLevel,
                                     i+1, maxSize);
      maxFixedValue = std::max(maxFixedValue,maxSegment);
    }
    return maxFixedValue;
  }
}

/// Pack tensor coordinates into an index structure and value array.  The
/// indices consist of one index per tensor mode, and each index contains
/// [0,2] index arrays.
int packTensor(const vector<int>& dimensions,
                const vector<TypedIndexVector>& coords,
                char* vals,
                size_t begin, size_t end,
                const vector<ModeType>& modeTypes, size_t i,
                std::vector<std::vector<TypedIndexVector>>* indices,
                char* values, DataType dataType, int valuesIndex) {
  auto& modeType    = modeTypes[i];
  auto& levelCoords = coords[i];
  auto& index       = (*indices)[i];
  switch (modeType) {
    case Dense: {
      // Iterate over each index value and recursively pack it's segment
      size_t cbegin = begin;
      for (int j=0; j < (int)dimensions[i]; ++j) {
        // Scan to find segment range of children
        size_t cend = cbegin;
        while (cend < end && levelCoords[cend] == j) {
          cend++;
        }
        PACK_NEXT_LEVEL(cend);
        cbegin = cend;
      }
      break;
    }
    case Sparse: {
      auto indexValues = getUniqueEntries(levelCoords, begin, end);

      // Store segment end: the size of the stored segment is the number of
      // unique values in the coordinate list
      index[0].push_back(index[1].size() + indexValues.size());

      // Store unique index values for this segment
      index[1].push_back_vector(indexValues);

      // Iterate over each index value and recursively pack it's segment
      size_t cbegin = begin;
      for (int j = 0; j < (int) indexValues.size(); j++) {
        // Scan to find segment range of children
        size_t cend = cbegin;
        while (cend < end && levelCoords[cend] == indexValues[j]) {
          cend++;
        }
        PACK_NEXT_LEVEL(cend);
        cbegin = cend;
      }
      break;
    }
    case Fixed: {
      TypedIndexVal fixedValue = index[0][0];
      auto indexValues = getUniqueEntries(levelCoords, begin, end);

      // Store segment end: the size of the stored segment is the number of
      // unique values in the coordinate list
      size_t segmentSize = indexValues.size();
      // Store unique index values for this segment
      size_t cbegin = begin;
      if (segmentSize > 0) {
        index[1].push_back_vector(indexValues);
        for (int j = 0; j < (int) indexValues.size(); j++) {
          // Scan to find segment range of children
          size_t cend = cbegin;
          while (cend < end && levelCoords[cend] == indexValues[j]) {
            cend++;
          }
          PACK_NEXT_LEVEL(cend);
          cbegin = cend;
        }
      }
      // Complete index if necessary with the last index value
      int curSize=segmentSize;
      while (fixedValue > curSize) {
        if (segmentSize > 0) {
          index[1].push_back(indexValues[segmentSize - 1]);
        }
        else {
          index[1].push_back(0);
        }
        PACK_NEXT_LEVEL(cbegin);
        curSize++;
      }
      break;
    }
  }
  return valuesIndex;
}
  
/// Pack tensor coordinates into a format. The coordinates must be stored as a
/// structure of arrays, that is one vector per axis coordinate and one vector
/// for the values. The coordinates must be sorted lexicographically.
Storage pack(const std::vector<int>&              dimensions,
             const Format&                        format,
             const std::vector<TypedIndexVector>& coordinates,
             const void *            values,
             const size_t numCoordinates,
             DataType datatype) {
  taco_iassert(dimensions.size() == format.getOrder());

  Storage storage(format);

  size_t order = dimensions.size();

  // Create vectors to store pointers to indices/index sizes
  vector<vector<TypedIndexVector>> indices;
  indices.reserve(order);

  for (size_t i=0; i < order; ++i) {
    switch (format.getModeTypes()[i]) {
      case Dense: {
        indices.push_back({});
        break;
      }
      case Sparse: {
        // Sparse indices have two arrays: a segment array and an index array
        indices.push_back({TypedIndexVector(format.getCoordinateTypePos(i)), TypedIndexVector(format.getCoordinateTypeIdx(i))});

        // Add start of first segment
        indices[i][0].push_back(0);
        break;
      }
      case Fixed: {
        // Fixed indices have two arrays: a segment array and an index array
        indices.push_back({TypedIndexVector(format.getCoordinateTypePos(i)), TypedIndexVector(format.getCoordinateTypeIdx(i))});

        // Add maximum size to segment array
        int maxSize = (int) findMaxFixedValue(dimensions, coordinates,
                                           format.getOrder(), i, 0,
                                           numCoordinates);
        taco_iassert(maxSize <= INT_MAX);
        indices[i][0].push_back(maxSize);
        break;
      }
    }
  }

  int max_size = 1;
  for (int i : dimensions)
    max_size *= i;

  void* vals = malloc(max_size * datatype.getNumBytes()); //has zeroes where dense
  int actual_size = packTensor(dimensions, coordinates, (char *) values, 0,
             numCoordinates, format.getModeTypes(), 0, &indices, (char *) vals, datatype, 0);

  vals = realloc(vals, actual_size);

  // Create a tensor index
  vector<ModeIndex> modeIndices;
  for (size_t i = 0; i < order; i++) {
    ModeType modeType = format.getModeTypes()[i];
    switch (modeType) {
      case ModeType::Dense: {
        Array size = makeArray({dimensions[i]});
        modeIndices.push_back(ModeIndex({size}));
        break;
      }
      case ModeType::Sparse:
      case ModeType::Fixed: {
        Array pos = makeArray(format.getCoordinateTypePos(i), indices[i][0].size());
        memcpy(pos.getData(), indices[i][0].data(), indices[i][0].size() * format.getCoordinateTypePos(i).getNumBytes());

        Array idx = makeArray(format.getCoordinateTypeIdx(i), indices[i][1].size());
        memcpy(idx.getData(), indices[i][1].data(), indices[i][1].size() * format.getCoordinateTypeIdx(i).getNumBytes());
        modeIndices.push_back(ModeIndex({pos, idx}));
        break;
      }
    }
  }
  storage.setIndex(Index(format, modeIndices));
  Array array = makeArray(datatype, actual_size/datatype.getNumBytes());
  memcpy(array.getData(), vals, actual_size);
  storage.setValues(array);
  free(vals);
  return storage;
}


ir::Stmt packCode(const Format& format) {
  using namespace taco::ir;

  vector<Stmt> packStmts;

  // Generate loops to count the size of each level.
//  ir::Stmt countLoops;
//  for (auto& level : util::reverse(format.getLevels())) {
//    if (countLoops.defined()) {
//
//    }
//  }

  // Loops to insert index values
  Stmt insertLoop;
  for (ModeType modeType : util::reverse(format.getModeTypes())) {
    Stmt body = insertLoop.defined()
        ? insertLoop
        : VarAssign::make(Var::make("test", Int()), 1.0, true);

    switch (modeType) {
      case Dense: {
        Expr dimension = (long long) 10;
        Expr loopVar = Var::make("i", Int());
        insertLoop = ir::For::make(loopVar, (long long) 0, dimension, (long long) 1, body);
        break;
      }
      case Sparse: {
        break;
      }
      case Fixed: {
        taco_not_supported_yet;
        break;
      }
    }
  }
  packStmts.push_back(insertLoop);

  return ir::Block::make(packStmts);
}

}}
