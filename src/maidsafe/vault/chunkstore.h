/*
 * copyright maidsafe.net limited 2008
 * The following source code is property of maidsafe.net limited and
 * is not meant for external use. The use of this code is governed
 * by the license file LICENSE.TXT found in teh root of this directory and also
 * on www.maidsafe.net.
 *
 * You are not free to copy, amend or otherwise use this source code without
 * explicit written permission of the board of directors of maidsafe.net
 *
 *  Created on: Sep 29, 2008
 *      Author: Haiyang, Jose
 */

#ifndef MAIDSAFE_VAULT_CHUNKSTORE_H_
#define MAIDSAFE_VAULT_CHUNKSTORE_H_

#include <boost/filesystem.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/mutex.hpp>
#include <list>
#include <map>
#include <string>

#include "gtest/gtest_prod.h"

#include "maidsafe/crypto.h"
#include "maidsafe/maidsafe.h"
#include "maidsafe/maidsafe-dht.h"

namespace fs = boost::filesystem;

namespace maidsafe_vault {

// ChunkType defines which directory chunk is held in.  Chunks must be exactly
// one of hashable or non-hashable and also exactly one of normal, cache,
// outgoing or tempcache.
typedef char ChunkType;
const ChunkType kHashable = 0x10;
const ChunkType kNonHashable = 0x20;
const ChunkType kNormal = 0x01;
const ChunkType kCache = 0x02;
const ChunkType kOutgoing = 0x04;
const ChunkType kTempCache = 0x08;

struct ChunkInfo {
  ChunkInfo() : non_hex_name_(""),
                last_checked_(boost::posix_time::min_date_time),
                type_(kHashable | kNormal) {}
  ChunkInfo(const std::string &non_hex_name,
            const boost::posix_time::ptime &last_checked,
            const ChunkType &type)
                : non_hex_name_(non_hex_name),
                  last_checked_(last_checked),
                  type_(type) {}
  std::string non_hex_name_;
  boost::posix_time::ptime last_checked_;
  ChunkType type_;
  bool operator<(const ChunkInfo &c) const
      {return non_hex_name_ < c.non_hex_name_;}
};

// multi_index_container tag
struct non_hex_name {};
struct last_checked {};
struct chunk_type {};
typedef boost::multi_index::multi_index_container<
    ChunkInfo,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<
            boost::multi_index::tag<non_hex_name>,
            BOOST_MULTI_INDEX_MEMBER(ChunkInfo, std::string, non_hex_name_)>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<last_checked>,
            BOOST_MULTI_INDEX_MEMBER(ChunkInfo, boost::posix_time::ptime,
                                     last_checked_)>,
        boost::multi_index::sequenced<>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<chunk_type>,
            BOOST_MULTI_INDEX_MEMBER(ChunkInfo, ChunkType, type_)>
        >
    > chunk_set;
typedef chunk_set::index<non_hex_name>::type chunk_set_by_non_hex_name;
typedef chunk_set::index<last_checked>::type chunk_set_by_last_checked;
typedef chunk_set::index<chunk_type>::type chunk_set_by_chunk_type;
typedef std::map<ChunkType, fs::path>::iterator path_map_iterator;

struct change_last_checked {
  explicit change_last_checked(const boost::posix_time::ptime &new_time)
      : new_time_(new_time) {}
  void operator()(ChunkInfo &chunk) {  // NOLINT (Fraser)
    chunk.last_checked_ = new_time_;
  }
 private:
  boost::posix_time::ptime new_time_;
};

struct change_type {
  explicit change_type(const ChunkType &new_type) : new_type_(new_type) {}
  void operator()(ChunkInfo &chunk) {  // NOLINT (Fraser)
    chunk.type_ = new_type_;
  }
 private:
  ChunkType new_type_;
};

class ChunkStore {
 public:
  explicit ChunkStore(const std::string &chunkstore_dir);
  ~ChunkStore() {}
  bool HasChunk(const std::string &key);
  bool StoreChunk(const std::string &key, const std::string &value);
  bool DeleteChunk(const std::string &key);
  // This replaces the existing value - it doesn't append to the existing value
  bool UpdateChunk(const std::string &key, const std::string &value);
  bool LoadChunk(const std::string &key, std::string *value);
  // Loads a chunk chosen at random from hashable normal (ie not cached) chunks
  bool LoadRandomChunk(std::string *key, std::string *value);
  void GetAllChunks(std::list<std::string> *chunk_names);
  // Check that hash of value == key
  int HashCheckChunk(const std::string &key);
  // Hash check all local chunks and add to list those that fail.  Bool set to
  // true causes failed chunks to be deleted.
  int HashCheckAllChunks(bool delete_failures,
                         std::list<std::string> *failed_keys);
  // By changing the chunk's type it will be moved to the appropriate directory
  int ChangeChunkType(const std::string &key, ChunkType type);
  bool is_initialised();
 private:
  ChunkStore(const ChunkStore&);
  ChunkStore& operator=(const ChunkStore&);
  FRIEND_TEST(TestChunkstore, BEH_MAID_ChunkstoreStoreChunk);
  FRIEND_TEST(TestChunkstore, BEH_MAID_ChunkstoreDeleteChunk);
  FRIEND_TEST(TestChunkstore, BEH_MAID_ChunkstoreLoadRandomChunk);
  FRIEND_TEST(TestChunkstore, BEH_MAID_ChunkstoreReuseDirectory);
  FRIEND_TEST(TestChunkstore, BEH_MAID_ChunkstoreHashCheckChunk);
  FRIEND_TEST(TestChunkstore, BEH_MAID_ChunkstoreChangeChunkType);
  FRIEND_TEST(TestChunkstore, BEH_MAID_ChunkstoreCheckAllChunks);
  void set_is_initialised(bool value);
  bool Init();
  // Populate map of <ChunkType, path to chunk root directory>
  bool PopulatePathMap();
  // Directory iterator
  void FindFiles(const fs::path &root_dir_path,
                 ChunkType type,
                 bool hash_check,
                 bool delete_failures,
                 boost::uint64_t *filecount,
                 std::list<std::string> *failed_keys);
  // Iterates through dir_path & fills chunkstore_set_
  bool PopulateChunkSet(ChunkType type, const fs::path &dir_path);
  ChunkType GetChunkType(const std::string &key, const std::string &value);
  // If the root path exists for that key, the bool create_path has no effect.
  // If the root path doesn't exist and create_path is true, the path will be
  // created and returned.  If the root path doesn't exist and create_path is
  // false, no action is taken other than to return an empty path.
  fs::path GetChunkPath(const std::string &key,
                        ChunkType type,
                        bool create_path);
  // Returns ChunkInfo for the chunk which was checked the longest time ago.  If
  // several chunks qualify, only the first one is returned.
  ChunkInfo GetOldestChecked();
  bool StoreChunkFunction(const std::string &key,
                          const std::string &value,
                          const fs::path &chunk_path,
                          ChunkType type);
  bool DeleteChunkFunction(const std::string &key, const fs::path &chunk_path);
  // Check that hash of value == key for appropriate chunks
  int HashCheckChunk(const std::string &key, const fs::path &chunk_path);
  chunk_set chunkstore_set_;
  std::map<ChunkType, fs::path> path_map_;
  const fs::path kChunkstorePath_;
  bool is_initialised_;
  boost::mutex initialised_mutex_, chunkstore_set_mutex_;
  // Leafs of directory paths that make up chunkstore
  const std::string kHashableLeaf_, kNonHashableLeaf_;
  const std::string kNormalLeaf_, kCacheLeaf_, kOutgoingLeaf_, kTempCacheLeaf_;
};
}  // namespace maidsafe_vault

#endif  // MAIDSAFE_VAULT_CHUNKSTORE_H_
