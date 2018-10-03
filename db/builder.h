// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_BUILDER_H_
#define STORAGE_LEVELDB_DB_BUILDER_H_

#include "hyperleveldb/status.h"
#include <vector>
#include <set>
#include <stdint.h>
#include "port/port.h"

namespace leveldb {

struct Options;
struct FileMetaData;

class Env;
class Iterator;
class TableCache;
class VersionEdit;
class VersionSet;

// Build a Table file from the contents of *iter.  The generated file
// will be named according to meta->number.  On success, the rest of
// *meta will be filled with metadata about the generated table.
// If no data is present in *iter, meta->file_size will be set to
// zero, and no Table file will be produced.
extern Status BuildTable(const std::string& dbname,
                         Env* env,
                         const Options& options,
                         TableCache* table_cache,
                         Iterator* iter,
                         FileMetaData* meta);

extern Status BuildTables(const std::string& dbname,
                         Env* env,
                         const Options& options,
                         VersionSet* versions,
                         TableCache* table_cache,
                         Iterator* iter,
                         std::vector< std::pair<int, FileMetaData> >& p_fm,
                         std::set<uint64_t> &pending_outputs,
                         port::Mutex* mutex_);    //5.7 添加锁

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_BUILDER_H_
