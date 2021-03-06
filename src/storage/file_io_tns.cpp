#include "taco/storage/file_io_tns.h"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <cmath>
#include <limits.h>

#include "taco/tensor.h"
#include "taco/format.h"
#include "taco/error.h"
#include "taco/util/strings.h"
#include "taco/util/files.h"

using namespace std;

namespace taco {

TensorBase readTNS(std::string filename, const Format& format, bool pack) {
  std::fstream file;
  util::openStream(file, filename, fstream::in);
  TensorBase tensor = readTNS(file, format, pack);
  file.close();
  return tensor;
}

TensorBase readTNS(std::istream& stream, const Format& format, bool pack) {
  std::vector<int>    coordinates;
  std::vector<double> values;

  std::string line;
  if (!std::getline(stream, line)) {
    return TensorBase();
  }

  // Infer tensor order from the first coordinate
  vector<string> toks = util::split(line, " ");
  size_t order = toks.size()-1;
  std::vector<int> dimensions(order);
  std::vector<int> coordinate(order);

  // Load data
  do {
    char* linePtr = (char*)line.data();
    for (size_t i = 0; i < order; i++) {
      long idx = strtol(linePtr, &linePtr, 10);
      taco_uassert(idx <= INT_MAX)<<"Coordinate in file is larger than INT_MAX";
      coordinate[i] = (int)idx - 1;
      dimensions[i] = std::max(dimensions[i], (int)idx);
    }
    coordinates.insert(coordinates.end(), coordinate.begin(), coordinate.end());
    double val = strtod(linePtr, &linePtr);
    values.push_back(val);

  } while (std::getline(stream, line));

  // Create tensor
  const size_t nnz = values.size();
  TensorBase tensor(type<double>(), dimensions, format);
  tensor.reserve(nnz);

  // Insert coordinates (TODO add and use bulk insertion)
  for (size_t i = 0; i < nnz; i++) {
    for (size_t j = 0; j < order; j++) {
      coordinate[j] = coordinates[i*order + j];
    }
    tensor.insert(coordinate, values[i]);
  }

  if (pack) {
    tensor.pack();
  }

  return tensor;
}

void writeTNS(std::string filename, const TensorBase& tensor) {
  std::fstream file;
  util::openStream(file, filename, fstream::out);
  writeTNS(file, tensor);
  file.close();
}

void writeTNS(std::ostream& stream, const TensorBase& tensor) {
  for (auto& value : iterate<double>(tensor)) {
    for (size_t coord : value.first) {
      stream << coord+1 << " ";
    }
    stream << value.second << endl;
  }
}

}
