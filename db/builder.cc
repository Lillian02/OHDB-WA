// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//3.23
#include <sstream>
#include <iostream>

#include "db/builder.h"

#include "db/filename.h"
#include "db/dbformat.h"
#include "db/table_cache.h"
#include "db/version_edit.h"
#include "db/version_set.h"
#include "hyperleveldb/db.h"
#include "hyperleveldb/env.h"
#include "hyperleveldb/iterator.h"
#include "port/port.h"

namespace leveldb {

Status BuildTable(const std::string& dbname,
                  Env* env,
                  const Options& options,
                  TableCache* table_cache,
                  Iterator* iter,
                  FileMetaData* meta) {
  Status s;
  meta->file_size = 0;
  iter->SeekToFirst();

  std::string fname = TableFileName(dbname, meta->number);
  if (iter->Valid()) {
    WritableFile* file;
    s = env->NewWritableFile(fname, &file);
    if (!s.ok()) {
      return s;
    }

    TableBuilder* builder = new TableBuilder(options, file);
    meta->smallest.DecodeFrom(iter->key());
    for (; iter->Valid(); iter->Next()) {
      Slice key = iter->key();
      meta->largest.DecodeFrom(key);
      builder->Add(key, iter->value());
    }

    // Finish and check for builder errors
    if (s.ok()) {
      s = builder->Finish();
      if (s.ok()) {
        meta->file_size = builder->FileSize();
        assert(meta->file_size > 0);
      }
    } else {
      builder->Abandon();
    }
    delete builder;

    // Finish and check for file errors
    if (s.ok()) {
      s = file->Sync();
    }
    if (s.ok()) {
      s = file->Close();
    }
    delete file;
    file = NULL;

    if (s.ok()) {
      // Verify that the table is usable
      Iterator* it = table_cache->NewIterator(ReadOptions(),
                                              meta->number,
                                              meta->file_size);
      s = it->status();
      delete it;
    }
  }

  // Check for input iterator errors
  if (!iter->status().ok()) {
    s = iter->status();
  }

  if (s.ok() && meta->file_size > 0) {
    // Keep it
  } else {
    env->DeleteFile(fname);
  }
  return s;
}

//1.29
Status BuildTables(const std::string& dbname,
                  Env* env,
                  const Options& options,
                  VersionSet* versions,
                  TableCache* table_cache,
                  Iterator* iter,
                  std::vector< std::pair<int, FileMetaData> >& p_fm,
                  std::set<uint64_t> &pending_outputs,
                  port::Mutex* mutex_) {  //5.7 添加锁
  Status s;

  iter->SeekToFirst();

  if (iter->Valid()) {

    std::string fname;
    WritableFile* file;
    TableBuilder* builder; 
    FileMetaData meta;
    std::string pre_thn = "first";
    std::string thn;
    int par;
    bool firstF = true;
    for (; iter->Valid(); iter->Next()) {     
      Slice key = iter->key();     
      //const char* entry = key.data();
      //uint32_t key_length;
      //const char* key_ptr = GetVarint32Ptr(entry, entry+5, &key_length);
      //std::cout << key_ptr << '\n';
      thn = key.data();
      thn = thn.substr(8,1);      //从第7位开始取3位，是前7位为0[根据key生成规则，可表示10亿个key]，分区为1000,取key的时候貌似前面截多了一位
      //std::cout << thn << '\n';
      if (pre_thn == thn){
        meta.largest.DecodeFrom(key);
        builder->Add(key, iter->value());
      } else{
        if (firstF){    //3.27 pre_thn.find("first") != std::string::npos表示在pre_thn中能找到first，即第一个KV，需要新建文件
          //the first one
          firstF = false;   //3.27 用于表示非第一个文件，加快判断速度
        }else{
          //一个分区结束，flush文件
          s = builder->Finish();
          if (s.ok()) {          
            meta.file_size = builder->FileSize();            
            assert(meta.file_size > 0);
          }else{
            builder->Abandon();
          }
  
          delete builder;
          builder = NULL;
      
          // Finish and check for file errors
          if (s.ok()) {
            s = file->Sync();
          }
          if (s.ok()) {
            s = file->Close();
          }
          delete file;
          file = NULL;
      
          if (s.ok()) {
            // Verify that the table is usable
            Iterator* it = table_cache->NewIterator(ReadOptions(),
              meta.number, meta.file_size);
            s = it->status();
            delete it;
          } 
  
          // Check for input iterator errors
          if (!iter->status().ok()) {
            s = iter->status();
          }
  
          if (s.ok() && meta.file_size > 0) {
            // Keep it
            //metas.push_back(meta);           
            p_fm.push_back(std::make_pair(par, meta));
          } else {
            env->DeleteFile(fname);
            return s;
          }
          Log(options.info_log, "Level-0 Partition-%d table #%llu: %lld bytes %s",
            par,   //3.27 增加分区元数据
            (unsigned long long) meta.number,
            (unsigned long long) meta.file_size,
            s.ToString().c_str());
          /*
          int level = 0;
          if (meta.file_size > 0) {
            const Slice min_user_key = meta.smallest.user_key();
            const Slice max_user_key = meta.largest.user_key();
            if (base != NULL) {
              level = base->PickLevelForMemTableOutput(min_user_key, max_user_key);
              while (level > 0 && levels_locked[level]) {
                --level;
              }
            }
            edit->AddFile(level, meta.number, meta.file_size,
                          meta.smallest, meta.largest);
          }

          CompactionStats stats;
          stats.micros = env_->NowMicros() - start_micros;
          stats.bytes_written = meta.file_size;
          stats_[level].Add(stats);
           
          pending_outputs.erase(meta.number);
          */
        }
        //3.26 新建文件
        //5.7 新建文件时带上锁
        mutex_->Lock();
        meta.number = versions->NewFileNumber();          
        meta.file_size = 0;
        meta.smallest.DecodeFrom(key);
        meta.largest.DecodeFrom(key);
        //2.2
        //mutex_.Lock();
        pending_outputs.insert(meta.number);
        //mutex_.Unlock();
        mutex_->Unlock();    //5.7 解锁
        fname = TableFileName(dbname, meta.number);
        s = env->NewWritableFile(fname, &file);
        if (!s.ok()) {
          return s;
        }
        builder = new TableBuilder(options, file);
        builder->Add(key, iter->value());
        pre_thn = thn;
        //将string转换成int的分区号，放到L0对应的bucket中            
        std::stringstream ss(pre_thn);
        ss>>par;
        Log(options.info_log, "Level-0 Partition-%d table #%llu: started", par,
          (unsigned long long) meta.number);               
                       
      }

    }
    
    //the last file
    s = builder->Finish();
    if (s.ok()) {          
      meta.file_size = builder->FileSize();
      
      assert(meta.file_size > 0);
    }else{
      builder->Abandon();
    }

    delete builder;
    builder = NULL;

    // Finish and check for file errors
    if (s.ok()) {
      s = file->Sync();
    }
    if (s.ok()) {
      s = file->Close();
    }
    delete file;
    file = NULL;

    if (s.ok()) {
      // Verify that the table is usable
      Iterator* it = table_cache->NewIterator(ReadOptions(),
        meta.number, meta.file_size);
      s = it->status();
      delete it;
    } 

    // Check for input iterator errors
    if (!iter->status().ok()) {
      s = iter->status();
    }

    if (s.ok() && meta.file_size > 0) {
      // Keep it
      int par;
      std::stringstream ss(pre_thn);
      ss>>par;
      p_fm.push_back(std::make_pair(par, meta));
    } else {
      env->DeleteFile(fname);
      return s;
    }
    Log(options.info_log, "Level-0 Partition-%d table #%llu: %lld bytes %s",
      par,  //3.27
      (unsigned long long) meta.number,
      (unsigned long long) meta.file_size,
      s.ToString().c_str());
    /*
    int level = 0;
    if (meta.file_size > 0) {
      const Slice min_user_key = meta.smallest.user_key();
      const Slice max_user_key = meta.largest.user_key();
      if (base != NULL) {
        level = base->PickLevelForMemTableOutput(min_user_key, max_user_key);
        while (level > 0 && levels_locked[level]) {
          --level;
        }
      }
      edit->AddFile(level, meta.number, meta.file_size,
                    meta.smallest, meta.largest);
    }

    CompactionStats stats;
    stats.micros = env_->NowMicros() - start_micros;
    stats.bytes_written = meta.file_size;
    stats_[level].Add(stats);
     
    pending_outputs.erase(meta.number);          
    */
  }
  
  return s;
}

}  // namespace leveldb
