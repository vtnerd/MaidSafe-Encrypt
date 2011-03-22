/*******************************************************************************
 *  Copyright 2008-2011 maidsafe.net limited                                   *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the license   *
 *  file LICENSE.TXT found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ***************************************************************************//**
 * @file  test_self_encryption.cc
 * @brief Tests for the self-encryption engine.
 * @date  2008-09-09
 */

#include <cstdint>
#include <iostream>  // NOLINT
#include <memory>
#include <sstream>

#include "boost/archive/text_oarchive.hpp"
#include "boost/archive/text_iarchive.hpp"
#include "boost/filesystem.hpp"
#include "boost/filesystem/fstream.hpp"
#include "boost/timer.hpp"
#include "gtest/gtest.h"
#include "maidsafe/common/crypto.h"
#include "maidsafe/common/utils.h"
#include "maidsafe-encrypt/self_encryption.h"
#include "maidsafe-encrypt/utils.h"

namespace fs = boost::filesystem;

namespace maidsafe {

namespace encrypt {

namespace test {

namespace test_se {

fs::path CreateRandomFile(const fs::path &file_path,
                          const std::uint64_t &file_size) {
  fs::ofstream ofs(file_path, std::ios::binary | std::ios::out |
                              std::ios::trunc);
  if (file_size != 0) {
    size_t string_size = (file_size > 100000) ? 100000 :
                        static_cast<size_t>(file_size);
    std::uint64_t remaining_size = file_size;
    std::string rand_str = RandomString(2 * string_size);
    std::string file_content;
    std::uint64_t start_pos = 0;
    while (remaining_size) {
      srand(17);
      start_pos = rand() % string_size;  // NOLINT (Fraser)
      if (remaining_size < string_size) {
        string_size = static_cast<size_t>(remaining_size);
        file_content = rand_str.substr(0, string_size);
      } else {
        file_content = rand_str.substr(static_cast<size_t>(start_pos),
                                       string_size);
      }
      ofs.write(file_content.c_str(), file_content.size());
      remaining_size -= string_size;
    }
  }
  ofs.close();
  return file_path;
}

std::uint64_t TotalChunkSize(const std::vector<std::uint32_t> &chunk_sizes) {
  std::uint64_t total(0);
  for (size_t i = 0; i < chunk_sizes.size(); ++i)
    total += chunk_sizes[i];
  return total;
}

size_t CountUniqueChunks(const DataMap &data_map) {
  std::set<std::string> chunks;
  for (auto it = data_map.chunks.begin(); it != data_map.chunks.end(); ++it)
    chunks.insert(it->hash);
  return chunks.size();
}

}  // namespace test_se

class SelfEncryptionTest : public testing::Test {
 public:
  SelfEncryptionTest()
      : root_dir_(),
        file_dir_(),
        chunk_dir_() {
    boost::system::error_code ec;
    root_dir_ = boost::filesystem::temp_directory_path(ec) /
        ("maidsafe_TestSE_" + RandomAlphaNumericString(6));
    file_dir_ = root_dir_ / "Files";
    chunk_dir_ = root_dir_ / "Chunks";
  }
  virtual ~SelfEncryptionTest() {}
 protected:
  void SetUp() {
    if (fs::exists(root_dir_))
      fs::remove_all(root_dir_);
    fs::create_directories(file_dir_);
    fs::create_directories(chunk_dir_);
  }
  void TearDown() {
    try {
      if (fs::exists(root_dir_))
        fs::remove_all(root_dir_);
    }
    catch(const std::exception& e) {
      printf("%s\n", e.what());
    }
  }
  testing::AssertionResult AssertStringsEqual(const char* expr1,
                                              const char* expr2,
                                              std::string s1,
                                              std::string s2) {
    if (s1 == s2)
      return testing::AssertionSuccess();

    const size_t kLineLength(76);

    s1 = EncodeToBase64(s1);
    if (s1.size() > kLineLength)
      s1 = s1.substr(0, kLineLength / 2 - 1) + ".." +
           s1.substr(s1.size() - kLineLength / 2 - 1);

    s2 = EncodeToBase64(s2);
    if (s2.size() > kLineLength)
      s2 = s2.substr(0, kLineLength / 2 - 1) + ".." +
           s2.substr(s2.size() - kLineLength / 2 - 1);

    return testing::AssertionFailure()
        << "Strings " << expr1 << " and " << expr2 << " are not equal: \n  "
        << s1 << "\n  " << s2;
  }

  fs::path root_dir_, file_dir_, chunk_dir_;
};

/*
class SelfEncryptionParamTest
  : public SelfEncryptionTest,
    public testing::WithParamInterface<SelfEncryptionParams> {};
  // TODO(Steve) add sep_ and param output
*/

// TODO(Steve) replace this by the above declaration after upgrade to gtest 1.6
class SelfEncryptionParamTest
  : public testing::TestWithParam<SelfEncryptionParams> {
 public:
  SelfEncryptionParamTest()
      : root_dir_(),
        file_dir_(),
        chunk_dir_(),
        sep_(GetParam()) {
    boost::system::error_code ec;
    root_dir_ = boost::filesystem::temp_directory_path(ec) /
        ("maidsafe_TestSE_" + RandomAlphaNumericString(6));
    file_dir_ = root_dir_ / "Files";
    chunk_dir_ = root_dir_ / "Chunks";
  }
  virtual ~SelfEncryptionParamTest() {}
 protected:
  void SetUp() {
    if (fs::exists(root_dir_))
      fs::remove_all(root_dir_);
    fs::create_directories(file_dir_);
    fs::create_directories(chunk_dir_);

    printf("Current SE parameters:\n"
           "  max chunk size            = %d Bytes\n"
           "  max includable chunk size = %d Bytes\n"
           "  max includable data size  = %d bytes\n",
           sep_.max_chunk_size,
           sep_.max_includable_chunk_size,
           sep_.max_includable_data_size);
  }
  void TearDown() {
    try {
      if (fs::exists(root_dir_))
        fs::remove_all(root_dir_);
    }
    catch(const std::exception& e) {
      printf("%s\n", e.what());
    }
  }
  testing::AssertionResult AssertStringsEqual(const char* expr1,
                                              const char* expr2,
                                              std::string s1,
                                              std::string s2) {
    if (s1 == s2)
      return testing::AssertionSuccess();

    const size_t kLineLength(76);

    s1 = EncodeToBase64(s1);
    if (s1.size() > kLineLength)
      s1 = s1.substr(0, kLineLength / 2 - 1) + ".." +
           s1.substr(s1.size() - kLineLength / 2 - 1);

    s2 = EncodeToBase64(s2);
    if (s2.size() > kLineLength)
      s2 = s2.substr(0, kLineLength / 2 - 1) + ".." +
           s2.substr(s2.size() - kLineLength / 2 - 1);

    return testing::AssertionFailure()
        << "Strings " << expr1 << " and " << expr2 << " are not equal: \n  "
        << s1 << "\n  " << s2;
  }

  fs::path root_dir_, file_dir_, chunk_dir_;
  const SelfEncryptionParams sep_;
};

// TODO(Steve) replace this by the above declaration after upgrade to gtest 1.6
class SelfEncryptionBenchmarkTest
  : public testing::TestWithParam<SelfEncryptionParams> {
 public:
  SelfEncryptionBenchmarkTest()
      : root_dir_(),
        file_dir_(),
        chunk_dir_(),
        sep_(GetParam()) {
    boost::system::error_code ec;
    root_dir_ = boost::filesystem::temp_directory_path(ec) /
        ("maidsafe_TestSE_" + RandomAlphaNumericString(6));
    file_dir_ = root_dir_ / "Files";
    chunk_dir_ = root_dir_ / "Chunks";
  }
  virtual ~SelfEncryptionBenchmarkTest() {}
 protected:
  void SetUp() {
    if (fs::exists(root_dir_))
      fs::remove_all(root_dir_);
    fs::create_directories(file_dir_);
    fs::create_directories(chunk_dir_);

    printf("Current SE parameters:\n"
           "  max chunk size            = %d Bytes\n"
           "  max includable chunk size = %d Bytes\n"
           "  max includable data size  = %d bytes\n",
           sep_.max_chunk_size,
           sep_.max_includable_chunk_size,
           sep_.max_includable_data_size);
  }
  void TearDown() {
    try {
      if (fs::exists(root_dir_))
        fs::remove_all(root_dir_);
    }
    catch(const std::exception& e) {
      printf("%s\n", e.what());
    }
  }

  fs::path root_dir_, file_dir_, chunk_dir_;
  const SelfEncryptionParams sep_;
};

TEST_F(SelfEncryptionTest, BEH_ENCRYPT_Serialisation) {
  DataMap data_map;
  {
    data_map.content = "abcdefg";
    data_map.size = 12345;
    ChunkDetails chunk;
    chunk.content = "test123";
    chunk.size = 10000;
    data_map.chunks.push_back(chunk);
    chunk.content = "test456";
    chunk.size = 2345;
    data_map.chunks.push_back(chunk);
  }
  std::stringstream ser_data_map;
  {  // serialise DataMap to string stream
    boost::archive::text_oarchive oa(ser_data_map);
    oa << data_map;
  }
  {
    DataMap restored_data_map;
    boost::archive::text_iarchive ia(ser_data_map);
    ia >> restored_data_map;
    EXPECT_EQ(data_map.content, restored_data_map.content);
    EXPECT_EQ(data_map.size, restored_data_map.size);
    EXPECT_EQ(data_map.chunks.size(), restored_data_map.chunks.size());
    EXPECT_EQ(data_map.chunks[0].content, restored_data_map.chunks[0].content);
    EXPECT_EQ(data_map.chunks[0].size, restored_data_map.chunks[0].size);
    EXPECT_EQ(data_map.chunks[1].content, restored_data_map.chunks[1].content);
    EXPECT_EQ(data_map.chunks[1].size, restored_data_map.chunks[1].size);
  }
}

TEST_F(SelfEncryptionTest, BEH_ENCRYPT_IsCompressedFile) {
  EXPECT_TRUE(utils::IsCompressedFile("test.7z"));
  EXPECT_TRUE(utils::IsCompressedFile("test.jpg"));
  EXPECT_TRUE(utils::IsCompressedFile("test.JPG"));
  EXPECT_TRUE(utils::IsCompressedFile("test.txt.rar"));
  EXPECT_TRUE(utils::IsCompressedFile("test.ZiP"));
  EXPECT_FALSE(utils::IsCompressedFile("test.txt"));
  EXPECT_FALSE(utils::IsCompressedFile("test.jpg.txt"));
}

TEST_F(SelfEncryptionTest, BEH_ENCRYPT_CheckCompressibility) {
  std::stringstream stream;

  // null pointer
  EXPECT_FALSE(utils::CheckCompressibility(NULL));

  // no data
  EXPECT_FALSE(utils::CheckCompressibility(&stream));
  stream.clear();

  //  make compressible string
  for (int i = 0; i < 1000; ++i)
    stream << "repeated text ";
  stream.seekg(stream.tellp() / 2);
  EXPECT_TRUE(utils::CheckCompressibility(&stream));

  //  make incompressible string
  stream.str("small text");
  stream.seekg(0);
  EXPECT_FALSE(utils::CheckCompressibility(&stream));
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_CalculateChunkSizes) {
  std::vector<std::uint32_t> chunk_sizes;
  std::uint64_t data_size(0);
  EXPECT_FALSE(utils::CalculateChunkSizes(data_size, sep_, NULL));
  EXPECT_FALSE(utils::CalculateChunkSizes(data_size, sep_, &chunk_sizes));
  EXPECT_EQ(0, chunk_sizes.size());

  chunk_sizes.clear();
  data_size = 1;
  EXPECT_FALSE(utils::CalculateChunkSizes(data_size, sep_, &chunk_sizes));
  EXPECT_EQ(0, chunk_sizes.size());

  chunk_sizes.clear();
  data_size = sep_.max_includable_data_size;
  EXPECT_FALSE(utils::CalculateChunkSizes(data_size, sep_, &chunk_sizes));
  EXPECT_EQ(0, chunk_sizes.size());

  chunk_sizes.clear();
  data_size = sep_.max_includable_data_size + 1;
  EXPECT_TRUE(utils::CalculateChunkSizes(data_size, sep_, &chunk_sizes));
  EXPECT_EQ(kMinChunks, chunk_sizes.size());
  EXPECT_EQ(data_size, test_se::TotalChunkSize(chunk_sizes));

  chunk_sizes.clear();
  data_size = sep_.max_chunk_size * kMinChunks - 1;
  if (data_size >= kMinChunks) {
    EXPECT_TRUE(utils::CalculateChunkSizes(data_size, sep_, &chunk_sizes));
    EXPECT_EQ(kMinChunks, chunk_sizes.size());
    EXPECT_EQ(data_size, test_se::TotalChunkSize(chunk_sizes));
  }

  chunk_sizes.clear();
  data_size = sep_.max_chunk_size * kMinChunks;
  EXPECT_TRUE(utils::CalculateChunkSizes(data_size, sep_, &chunk_sizes));
  EXPECT_EQ(kMinChunks, chunk_sizes.size());
  for (size_t i = 0; i < chunk_sizes.size(); ++i)
    EXPECT_EQ(sep_.max_chunk_size, chunk_sizes[i]);
  EXPECT_EQ(data_size, test_se::TotalChunkSize(chunk_sizes));

  chunk_sizes.clear();
  std::uint64_t base(RandomUint32() % 6 + 4),
                extra(RandomUint32() % sep_.max_chunk_size);
  data_size = base * sep_.max_chunk_size + extra;
  EXPECT_TRUE(utils::CalculateChunkSizes(data_size, sep_, &chunk_sizes));
  EXPECT_EQ(base + (extra > 0 ? 1 : 0), chunk_sizes.size());
  for (size_t i = 0; i < chunk_sizes.size(); ++i)
    if (i < chunk_sizes.size() - 1 || extra == 0)
      EXPECT_EQ(sep_.max_chunk_size, chunk_sizes[i]);
    else
      EXPECT_EQ(extra, chunk_sizes[i]);
  EXPECT_EQ(data_size, test_se::TotalChunkSize(chunk_sizes));
}

TEST_F(SelfEncryptionTest, BEH_ENCRYPT_ResizeObfuscationHash) {
  std::string input("abc");
  std::string hash = crypto::Hash<crypto::SHA512>(input);
  EXPECT_EQ(EncodeToHex(hash),
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a219299"
        "2a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
  std::string amended_hash("Rubbish");
  EXPECT_TRUE(utils::ResizeObfuscationHash(input, 65, &amended_hash));
  char appended(55);
  EXPECT_EQ(amended_hash, hash + appended);
  EXPECT_TRUE(utils::ResizeObfuscationHash(input, 10, &amended_hash));
  EXPECT_EQ(std::string("\xdd\xaf\x35\xa1\x93\x61\x7a\xba\xcc\x41"),
            amended_hash);
  EXPECT_TRUE(utils::ResizeObfuscationHash(input, 200, &amended_hash));
  EXPECT_EQ(std::string("\x91\xee\x3b\x36\xd\x3e\x5e\xe\xd\xe"),
            amended_hash.substr(190, 10));
  EXPECT_FALSE(utils::ResizeObfuscationHash(hash, 10, NULL));
}

TEST_F(SelfEncryptionTest, BEH_ENCRYPT_SelfEnDecryptChunk) {
  std::string content(RandomString(3000 + RandomUint32() % 1000));
  std::string hash1(RandomString(64)), hash2(RandomString(64));
  ASSERT_NE(hash1, hash2);

  EXPECT_PRED_FORMAT2(AssertStringsEqual, content, utils::SelfDecryptChunk(
      utils::SelfEncryptChunk(content, hash1, hash2), hash1, hash2));

  EXPECT_NE(content, utils::SelfDecryptChunk(
      utils::SelfEncryptChunk(content, hash1, hash2), hash2, hash1));
}

TEST_F(SelfEncryptionTest, BEH_ENCRYPT_SelfEnDecryptStreamInvalid) {
  // Invalid calls
  SelfEncryptionParams sep;
  DataMap data_map;
  std::istringstream stream;
  EXPECT_EQ(kInvalidInput,
            SelfEncrypt(&stream, chunk_dir_, false, sep, &data_map));
  stream.str("test");
  EXPECT_EQ(kNullPointer,
            SelfEncrypt(&stream, chunk_dir_, false, sep, NULL));
  EXPECT_EQ(kNullPointer,
            SelfEncrypt(NULL, chunk_dir_, false, sep, &data_map));
  EXPECT_EQ(kNullPointer, SelfDecrypt(data_map, chunk_dir_,
                                      static_cast<std::ostringstream*>(NULL)));
  sep = SelfEncryptionParams(0, 0, kMinChunks - 1);
  EXPECT_EQ(kInvalidInput,
            SelfEncrypt(&stream, chunk_dir_, false, sep, &data_map));
  sep = SelfEncryptionParams(1, 0, 0);
  EXPECT_EQ(kInvalidInput,
            SelfEncrypt(&stream, chunk_dir_, false, sep, &data_map));
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_SelfEnDecryptStreamTinyData) {
  {  // Only one byte of data
    DataMap data_map;
    std::istringstream stream_in(RandomString(1));
    std::string hash_in = crypto::Hash<crypto::SHA512>(stream_in.str());
    EXPECT_EQ(kSuccess, SelfEncrypt(&stream_in, chunk_dir_, false, sep_,
                                    &data_map));
    EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
    std::ostringstream stream_out;
    EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, &stream_out));
    ASSERT_PRED_FORMAT2(AssertStringsEqual, stream_in.str(), stream_out.str());
  }
  {  // Smallest amount of data to allow chunking
    DataMap data_map;
    std::istringstream stream_in(RandomString(kMinChunks));
    std::string hash_in = crypto::Hash<crypto::SHA512>(stream_in.str());
    EXPECT_EQ(kSuccess, SelfEncrypt(&stream_in, chunk_dir_, false, sep_,
                                    &data_map));
    EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
    std::ostringstream stream_out;
    EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, &stream_out));
    ASSERT_PRED_FORMAT2(AssertStringsEqual, stream_in.str(), stream_out.str());
  }
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_SelfEnDecryptStreamFullInclude) {
  // Little data, should end up completely in DM
  DataMap data_map;
  if (sep_.max_includable_data_size == 0)
    return;
  std::istringstream stream_in(RandomString(sep_.max_includable_data_size));
  std::string hash_in = crypto::Hash<crypto::SHA512>(stream_in.str());
  EXPECT_EQ(kSuccess, SelfEncrypt(&stream_in, chunk_dir_, false, sep_,
                                  &data_map));
  EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
  EXPECT_EQ(0, data_map.chunks.size());
  EXPECT_EQ(kNoCompression, data_map.compression_type);
  EXPECT_EQ(kObfuscate3AES256, data_map.self_encryption_type);
  EXPECT_EQ(sep_.max_includable_data_size, data_map.size);
  EXPECT_EQ(sep_.max_includable_data_size, data_map.content.size());
  EXPECT_EQ(hash_in, crypto::Hash<crypto::SHA512>(data_map.content));
  std::ostringstream stream_out;
  EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, &stream_out));
  ASSERT_PRED_FORMAT2(AssertStringsEqual, stream_in.str(), stream_out.str());
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_SelfEnDecryptStreamNoInclude) {
  // Data just big enough to chunk
  DataMap data_map;
  std::istringstream stream_in(RandomString(
      sep_.max_includable_data_size + 1));
  std::string hash_in = crypto::Hash<crypto::SHA512>(stream_in.str());
  EXPECT_EQ(kSuccess, SelfEncrypt(&stream_in, chunk_dir_, false, sep_,
                                  &data_map));
  EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
  EXPECT_EQ(kMinChunks, data_map.chunks.size());
  EXPECT_EQ(kNoCompression, data_map.compression_type);
  EXPECT_EQ(kObfuscate3AES256, data_map.self_encryption_type);
  EXPECT_EQ(sep_.max_includable_data_size + 1, data_map.size);
  EXPECT_TRUE(data_map.content.empty());
  std::uint64_t total_size(0);
  for (auto it = data_map.chunks.begin(); it < data_map.chunks.end(); ++it) {
    EXPECT_FALSE(it->hash.empty());
    fs::path chunk_path(chunk_dir_ / EncodeToHex(it->hash));
    EXPECT_TRUE(fs::exists(chunk_path));
    EXPECT_TRUE(it->content.empty());
    EXPECT_EQ(it->size, fs::file_size(chunk_path));
    EXPECT_GE(sep_.max_chunk_size, it->size);
    EXPECT_EQ(it->hash, crypto::HashFile<crypto::SHA512>(chunk_path));
    EXPECT_FALSE(it->pre_hash.empty());
    EXPECT_EQ(it->size, it->pre_size);  // no compression
    total_size += it->pre_size;
  }
  EXPECT_EQ(sep_.max_includable_data_size + 1, total_size);
  std::ostringstream stream_out;
  EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, &stream_out));
  ASSERT_PRED_FORMAT2(AssertStringsEqual, stream_in.str(), stream_out.str());
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_SelfEnDecryptStreamLastInclude) {
  // Last chunk ends up in DM
  DataMap data_map;
  if (sep_.max_includable_chunk_size == 0)
    return;
  std::uint64_t data_size(kMinChunks * sep_.max_chunk_size +
                          sep_.max_includable_chunk_size);
  std::istringstream stream_in(RandomString(data_size));
  std::string hash_in = crypto::Hash<crypto::SHA512>(stream_in.str());
  EXPECT_EQ(kSuccess, SelfEncrypt(&stream_in, chunk_dir_, false, sep_,
                                  &data_map));
  EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
  EXPECT_EQ(kMinChunks + 1, data_map.chunks.size());
  EXPECT_EQ(kNoCompression, data_map.compression_type);
  EXPECT_EQ(kObfuscate3AES256, data_map.self_encryption_type);
  EXPECT_EQ(data_size, data_map.size);
  EXPECT_TRUE(data_map.content.empty());
  std::uint64_t total_size(0);
  std::uint32_t i(0);
  for (auto it = data_map.chunks.begin(); it < data_map.chunks.end(); ++it) {
    if (i < kMinChunks) {
      // chunk is a file
      EXPECT_FALSE(it->hash.empty());
      fs::path chunk_path(chunk_dir_ / EncodeToHex(it->hash));
      EXPECT_TRUE(fs::exists(chunk_path));
      EXPECT_TRUE(it->content.empty());
      EXPECT_EQ(it->size, fs::file_size(chunk_path));
      EXPECT_EQ(sep_.max_chunk_size, it->size);
      EXPECT_EQ(it->hash, crypto::HashFile<crypto::SHA512>(chunk_path));
      EXPECT_FALSE(it->pre_hash.empty());
      EXPECT_EQ(it->size, it->pre_size);  // no compression
      total_size += it->pre_size;
    } else {
      // chunk is included in DataMap
      EXPECT_TRUE(it->hash.empty());
      EXPECT_FALSE(it->content.empty());
      EXPECT_EQ(sep_.max_includable_chunk_size, it->content.size());
      EXPECT_EQ(it->size, it->content.size());
      EXPECT_EQ(it->pre_size, it->content.size());  // no compression
      EXPECT_EQ(it->pre_hash, crypto::Hash<crypto::SHA512>(it->content));
      total_size += it->content.size();
    }
    ++i;
  }
  EXPECT_EQ(data_size, total_size);
  std::ostringstream stream_out;
  EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, &stream_out))
      << "Data size: " << data_size;
  ASSERT_PRED_FORMAT2(AssertStringsEqual, stream_in.str(), stream_out.str());
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_SelfEnDecryptStreamPattern) {
  // Check with different sequences of repeating chunks
  ASSERT_EQ(3, kMinChunks);  // chunk depends on following 2 chunks
  std::string chunk_a(RandomString(sep_.max_chunk_size));
  std::string chunk_b(RandomString(sep_.max_chunk_size));
  std::string chunk_c(RandomString(sep_.max_chunk_size));

  for (int i = 0; i <= 11; ++i) {
    DataMap data_map;
    std::istringstream stream_in;
    size_t expected_chunks(0);
    switch (i) {
      case 0:  // abc
        stream_in.str(chunk_a + chunk_b + chunk_c);
        expected_chunks = 3;
        break;
      case 1:  // aaa
        stream_in.str(chunk_a + chunk_a + chunk_a);
        expected_chunks = 1;
        break;
      case 2:  // aaaab
        stream_in.str(chunk_a + chunk_a + chunk_a + chunk_a + chunk_b);
        expected_chunks = 4;
        break;
      case 3:  // baaaa
        stream_in.str(chunk_b + chunk_a + chunk_a + chunk_a + chunk_a);
        expected_chunks = 4;
        break;
      case 4:  // aabaa
        stream_in.str(chunk_a + chunk_a + chunk_b + chunk_a + chunk_a);
        expected_chunks = 4;
        break;
      case 5:  // baaab
        stream_in.str(chunk_b + chunk_a + chunk_a + chunk_a + chunk_b);
        expected_chunks = 5;
        break;
      case 6:  // aaabc
        stream_in.str(chunk_a + chunk_a + chunk_a + chunk_b + chunk_c);
        expected_chunks = 5;
        break;
      case 7:  // aabaab
        stream_in.str(chunk_a + chunk_a + chunk_b + chunk_a + chunk_a +
                      chunk_b);
        expected_chunks = 3;
        break;
      case 8:  // aabaac
        stream_in.str(chunk_a + chunk_a + chunk_b + chunk_a + chunk_a +
                      chunk_c);
        expected_chunks = 6;
        break;
      case 9:  // aabaacaac
        stream_in.str(chunk_a + chunk_a + chunk_b + chunk_a + chunk_a +
                      chunk_c + chunk_a + chunk_a + chunk_c);
        expected_chunks = 6;
        break;
      case 10:  // aabaacaab
        stream_in.str(chunk_a + chunk_a + chunk_b + chunk_a + chunk_a +
                      chunk_c + chunk_a + chunk_a + chunk_b);
        expected_chunks = 6;
        break;
      case 11:  // abaca
        stream_in.str(chunk_a + chunk_b + chunk_a + chunk_c + chunk_a);
        expected_chunks = 5;
        break;
    }
    EXPECT_EQ(kSuccess, SelfEncrypt(&stream_in, chunk_dir_, false, sep_,
                                    &data_map));
    EXPECT_EQ(expected_chunks, test_se::CountUniqueChunks(data_map));
    EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
    std::ostringstream stream_out;
    EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, &stream_out));
    ASSERT_PRED_FORMAT2(AssertStringsEqual, stream_in.str(), stream_out.str())
        << "Case " << i;
  }
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_SelfEnDecryptStreamDedup) {
  // Check de-duplication (identical chunks except for last one)
  DataMap data_map;
  const size_t kChunkCount(5 * kMinChunks);
  std::string chunk_content(RandomString(sep_.max_chunk_size));
  std::string last_chunk_content(RandomString(sep_.max_chunk_size));
  std::string chunk_hash(crypto::Hash<crypto::SHA512>(chunk_content));
  std::string last_chunk_hash(crypto::Hash<crypto::SHA512>(last_chunk_content));
  ASSERT_NE(chunk_hash, last_chunk_hash);

  std::string data_content;
  for (size_t i = 0; i < kChunkCount - 1; ++i)
    data_content.append(chunk_content);
  data_content.append(last_chunk_content);
  std::istringstream stream_in(data_content);
  ASSERT_EQ(kChunkCount * sep_.max_chunk_size, stream_in.str().size());
  std::string hash_in = crypto::Hash<crypto::SHA512>(stream_in.str());
  EXPECT_EQ(kSuccess, SelfEncrypt(&stream_in, chunk_dir_, false, sep_,
                                  &data_map));
  EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
  EXPECT_EQ(kChunkCount, data_map.chunks.size());
  EXPECT_EQ(kNoCompression, data_map.compression_type);
  EXPECT_EQ(kObfuscate3AES256, data_map.self_encryption_type);
  EXPECT_EQ(kChunkCount * sep_.max_chunk_size, data_map.size);
  EXPECT_TRUE(data_map.content.empty());

  std::uint64_t total_size(0);
  std::string post_enc_hash;
  for (size_t i = 0; i < data_map.chunks.size(); ++i) {
    EXPECT_FALSE(data_map.chunks[i].hash.empty());
    if (i == 0)
      post_enc_hash = data_map.chunks[i].hash;
    else if (i < data_map.chunks.size() - kMinChunks)
      EXPECT_EQ(post_enc_hash, data_map.chunks[i].hash);
    else
      EXPECT_NE(post_enc_hash, data_map.chunks[i].hash);
    EXPECT_TRUE(data_map.chunks[i].content.empty());
    EXPECT_EQ(sep_.max_chunk_size, data_map.chunks[i].size);
    if (i < data_map.chunks.size() - 1)
      EXPECT_EQ(chunk_hash, data_map.chunks[i].pre_hash);
    else
      EXPECT_EQ(last_chunk_hash, data_map.chunks[i].pre_hash);
    EXPECT_EQ(data_map.chunks[i].size, data_map.chunks[i].pre_size);  // uncompr
    total_size += data_map.chunks[i].pre_size;
  }

  EXPECT_EQ(kChunkCount * sep_.max_chunk_size, total_size);
  std::ostringstream stream_out;
  EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, &stream_out));
  ASSERT_PRED_FORMAT2(AssertStringsEqual, stream_in.str(), stream_out.str());
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_SelfEnDecryptStreamCharacters) {
  // Try all possible characters
  // NOTE Test is needed because streams tend to choke on certain characters.
  for (int i = 0; i < 256; ++i) {
    DataMap data_map;
    std::uint64_t data_size(RandomUint32() % sep_.max_includable_data_size + 1);
    std::istringstream stream_in(std::string(data_size, static_cast<char>(i)));
    EXPECT_EQ(kSuccess, SelfEncrypt(&stream_in, chunk_dir_, false, sep_,
                                    &data_map));
    EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
    std::ostringstream stream_out;
    EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, &stream_out))
        << "Character: " << i << "\nData size: " << data_size;
    ASSERT_PRED_FORMAT2(AssertStringsEqual, stream_in.str(), stream_out.str());
  }
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_SelfEnDecryptStreamDelChunk) {
  // First chunk is deleted
  DataMap data_map;
  std::uint64_t data_size((RandomUint32() % kMinChunks + 1) *
                          sep_.max_chunk_size + kMinChunks +
                          RandomUint32() % sep_.max_chunk_size);
  std::istringstream stream_in(RandomString(data_size));
  EXPECT_EQ(kSuccess, SelfEncrypt(&stream_in, chunk_dir_, false, sep_,
                                  &data_map));
  EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
  EXPECT_LE(kMinChunks, data_map.chunks.size());
  EXPECT_TRUE(fs::remove(chunk_dir_ / EncodeToHex(data_map.chunks[0].hash)));
  EXPECT_FALSE(ChunksExist(data_map, chunk_dir_, NULL));
  std::ostringstream stream_out;
  ASSERT_EQ(kDecryptError, SelfDecrypt(data_map, chunk_dir_, &stream_out));
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_SelfEnDecryptStreamResizeChunk) {
  // First chunk is changed in size (and contents)
  DataMap data_map;
  std::uint64_t data_size((RandomUint32() % kMinChunks + 1) *
                          sep_.max_chunk_size + kMinChunks +
                          RandomUint32() % sep_.max_chunk_size);
  std::istringstream stream_in(RandomString(data_size));
  EXPECT_EQ(kSuccess, SelfEncrypt(&stream_in, chunk_dir_, false, sep_,
                                  &data_map));
  EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
  EXPECT_LE(kMinChunks, data_map.chunks.size());
  test_se::CreateRandomFile(
      chunk_dir_ / EncodeToHex(data_map.chunks[0].hash),
      data_map.chunks[0].size + 7);
  std::ostringstream stream_out;
  ASSERT_EQ(kDecryptError, SelfDecrypt(data_map, chunk_dir_, &stream_out));
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_SelfEnDecryptStreamCorruptChunk) {
  // First chunk is changed only in contents
  DataMap data_map;
  std::uint64_t data_size((RandomUint32() % kMinChunks + 1) *
                          sep_.max_chunk_size + kMinChunks +
                          RandomUint32() % sep_.max_chunk_size);
  std::istringstream stream_in(RandomString(data_size));
  EXPECT_EQ(kSuccess, SelfEncrypt(&stream_in, chunk_dir_, false, sep_,
                                  &data_map));
  EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
  EXPECT_LE(kMinChunks, data_map.chunks.size());
  fs::path chunk_path(chunk_dir_ / EncodeToHex(data_map.chunks[0].hash));
  while (crypto::HashFile<crypto::SHA512>(chunk_path) ==
         data_map.chunks[0].hash) {
    test_se::CreateRandomFile(chunk_path, data_map.chunks[0].size);
  }
  std::ostringstream stream_out;
  ASSERT_EQ(kDecryptError, SelfDecrypt(data_map, chunk_dir_, &stream_out));
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_SelfEnDecryptString) {
  {  // Invalid calls
    DataMap data_map;
    EXPECT_EQ(kInvalidInput, SelfEncrypt("", chunk_dir_, false, sep_,
                                         &data_map));
    EXPECT_EQ(kNullPointer, SelfEncrypt("test", chunk_dir_, false, sep_, NULL));
    EXPECT_EQ(kNullPointer, SelfDecrypt(data_map, chunk_dir_,
                                        static_cast<std::string*>(NULL)));
  }
  {  // Small data test
    DataMap data_map;
    std::uint64_t data_size(kMinChunks);
    std::string string_in(RandomString(data_size));
    EXPECT_EQ(kSuccess, SelfEncrypt(string_in, chunk_dir_, false, sep_,
                                    &data_map));
    EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
    std::string string_out;
    EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, &string_out))
        << "Data size: " << data_size;
    ASSERT_PRED_FORMAT2(AssertStringsEqual, string_in, string_out);
  }
  {  // Random data test
    DataMap data_map;
    std::uint64_t data_size((RandomUint32() % kMinChunks + 1) *
                            sep_.max_chunk_size + kMinChunks +
                            RandomUint32() % sep_.max_chunk_size);
    std::string string_in(RandomString(data_size));
    EXPECT_EQ(kSuccess, SelfEncrypt(string_in, chunk_dir_, false, sep_,
                                    &data_map));
    EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
    EXPECT_LE(kMinChunks, data_map.chunks.size());
    std::string string_out;
    EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, &string_out))
        << "Data size: " << data_size;
    ASSERT_PRED_FORMAT2(AssertStringsEqual, string_in, string_out);
  }
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_SelfEnDecryptFile) {
  fs::path path_in(file_dir_ / "SelfEncryptFilesTestIn.dat");
  fs::path path_out(file_dir_ / "SelfEncryptFilesTestOut.dat");

  {  // Invalid calls
    DataMap data_map;
    EXPECT_EQ(kInvalidInput, SelfEncrypt(path_in, chunk_dir_, sep_, &data_map));
    test_se::CreateRandomFile(path_in, 0);
    EXPECT_EQ(kInvalidInput, SelfEncrypt(path_in, chunk_dir_, sep_, &data_map));
    test_se::CreateRandomFile(path_in, 1);
    EXPECT_EQ(kNullPointer, SelfEncrypt(path_in, chunk_dir_, sep_, NULL));
  }
  {  // Small data test
    DataMap data_map;
    std::uint64_t data_size(kMinChunks);
    test_se::CreateRandomFile(path_in, data_size);
    EXPECT_EQ(kSuccess, SelfEncrypt(path_in, chunk_dir_, sep_, &data_map));
    EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
    EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, true, path_out))
        << "Data size: " << data_size;
    EXPECT_TRUE(fs::exists(path_out));
    ASSERT_PRED_FORMAT2(AssertStringsEqual,
                        crypto::HashFile<crypto::SHA512>(path_in),
                        crypto::HashFile<crypto::SHA512>(path_out));
  }
  {  // Random data test
    DataMap data_map;
    std::uint64_t data_size((RandomUint32() % kMinChunks + 1) *
                            sep_.max_chunk_size + kMinChunks +
                            RandomUint32() % sep_.max_chunk_size);
    test_se::CreateRandomFile(path_in, data_size);
    EXPECT_EQ(kSuccess, SelfEncrypt(path_in, chunk_dir_, sep_, &data_map));
    EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
    EXPECT_LE(kMinChunks, data_map.chunks.size());
    EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, true, path_out))
        << "Data size: " << data_size;
    EXPECT_TRUE(fs::exists(path_out));
    ASSERT_PRED_FORMAT2(AssertStringsEqual,
                        crypto::HashFile<crypto::SHA512>(path_in),
                        crypto::HashFile<crypto::SHA512>(path_out));
  }
  {  // Try restoring existing file
    DataMap data_map;
    EXPECT_EQ(kFileAlreadyExists,
              SelfDecrypt(data_map, chunk_dir_, false, path_out));
    EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, true, path_out));
  }
}

TEST_P(SelfEncryptionParamTest, BEH_ENCRYPT_SelfEnDecryptMixed) {
  {  // String input, file output
    DataMap data_map;
    std::uint64_t data_size((RandomUint32() % kMinChunks + 1) *
                            sep_.max_chunk_size + kMinChunks +
                            RandomUint32() % sep_.max_chunk_size);
    std::string string_in(RandomString(data_size));
    EXPECT_EQ(kSuccess, SelfEncrypt(string_in, chunk_dir_, false, sep_,
                                    &data_map));
    EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
    EXPECT_LE(kMinChunks, data_map.chunks.size());
    fs::path path_out(file_dir_ / "SelfEncryptFilesTestOut.dat");
    EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, true, path_out))
        << "Data size: " << data_size;
    EXPECT_TRUE(fs::exists(path_out));
    ASSERT_PRED_FORMAT2(AssertStringsEqual,
                        crypto::Hash<crypto::SHA512>(string_in),
                        crypto::HashFile<crypto::SHA512>(path_out));
  }

  {  // File input, string output
    DataMap data_map;
    std::uint64_t data_size((RandomUint32() % kMinChunks + 1) *
                            sep_.max_chunk_size + kMinChunks +
                            RandomUint32() % sep_.max_chunk_size);
    fs::path path_in(file_dir_ / "SelfEncryptFilesTestIn.dat");
    test_se::CreateRandomFile(path_in, data_size);
    EXPECT_EQ(kSuccess, SelfEncrypt(path_in, chunk_dir_, sep_, &data_map));
    EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
    EXPECT_LE(kMinChunks, data_map.chunks.size());
    std::string string_out;
    EXPECT_EQ(kSuccess, SelfDecrypt(data_map, chunk_dir_, &string_out))
        << "Data size: " << data_size;
    ASSERT_PRED_FORMAT2(AssertStringsEqual,
                        crypto::HashFile<crypto::SHA512>(path_in),
                        crypto::Hash<crypto::SHA512>(string_out));
  }
}

TEST_F(SelfEncryptionTest, BEH_ENCRYPT_ChunksExist) {
  DataMap data_map;
  EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, NULL));
  std::vector<std::string> missing_chunks;
  EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, &missing_chunks));
  EXPECT_TRUE(missing_chunks.empty());
  missing_chunks.push_back("test chunk name");
  EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, &missing_chunks));
  EXPECT_TRUE(missing_chunks.empty());
  {
    ChunkDetails chunk;
    chunk.hash = crypto::Hash<crypto::SHA512>("chunk1");
    data_map.chunks.push_back(chunk);
    chunk.hash = crypto::Hash<crypto::SHA512>("chunk2");
    data_map.chunks.push_back(chunk);
    chunk.hash = crypto::Hash<crypto::SHA512>("chunk3");
    data_map.chunks.push_back(chunk);
  }
  EXPECT_FALSE(ChunksExist(data_map, chunk_dir_, NULL));
  EXPECT_FALSE(ChunksExist(data_map, chunk_dir_, &missing_chunks));
  ASSERT_EQ(3, missing_chunks.size());
  EXPECT_EQ(data_map.chunks[0].hash, missing_chunks[0]);
  EXPECT_EQ(data_map.chunks[1].hash, missing_chunks[1]);
  EXPECT_EQ(data_map.chunks[2].hash, missing_chunks[2]);
  test_se::CreateRandomFile(chunk_dir_ / EncodeToHex(data_map.chunks[1].hash),
                            123);
  data_map.chunks[2].content = "chunk3";
  EXPECT_FALSE(ChunksExist(data_map, chunk_dir_, NULL));
  EXPECT_FALSE(ChunksExist(data_map, chunk_dir_, &missing_chunks));
  ASSERT_EQ(1, missing_chunks.size());
  EXPECT_EQ(data_map.chunks[0].hash, missing_chunks[0]);
  test_se::CreateRandomFile(chunk_dir_ / EncodeToHex(data_map.chunks[0].hash),
                            123);
  EXPECT_TRUE(ChunksExist(data_map, chunk_dir_, &missing_chunks));
  EXPECT_TRUE(missing_chunks.empty());
}

TEST_F(SelfEncryptionTest, DISABLED_BEH_ENCRYPT_Compression) {
  // TODO(Steve) Test if compression can be toggled, it's noticable in sizes,
  //             and resulting chunk sizes are constant except for the last one.
  FAIL() << "Not implemented yet.";
}

INSTANTIATE_TEST_CASE_P(VarChunkSizes, SelfEncryptionParamTest, testing::Values(
    SelfEncryptionParams(1, 0, kMinChunks - 1),  // 1 Byte
    SelfEncryptionParams(1 << 8, 1 << 5, 1 << 7),  // 256 Bytes
    SelfEncryptionParams(1 << 18, 1 << 8, 1 << 10)  // 256 KiB (default)
));

TEST_P(SelfEncryptionBenchmarkTest, FUNC_ENCRYPT_Benchmark) {
  const size_t kRunCount(16);
  for (size_t run = 0; run < kRunCount; ++run) {
    size_t repetitions(0);
    size_t data_size(64 << run);
    if (data_size <= (1 << 12))
      repetitions = 1000;
    else if (data_size <= (1 << 15))
      repetitions = 100;
    else
      repetitions = 10;

    printf("Timing Self-encryption of %d strings à %d bytes (run %d/%d)...\n",
           repetitions, data_size, run + 1, kRunCount);

    fs::create_directories(chunk_dir_);

    std::vector<std::shared_ptr<std::istringstream>> contents;
    std::vector<DataMap> data_maps;
    for (size_t i = 0; i < repetitions; ++i) {
      std::shared_ptr<std::istringstream> stream_ptr(
          new std::istringstream(RandomString(data_size)));
      contents.push_back(stream_ptr);
    }
    ASSERT_EQ(repetitions, contents.size());
    data_maps.resize(repetitions);

    boost::posix_time::ptime time =
        boost::posix_time::microsec_clock::universal_time();
    for (size_t i = 0; i < repetitions; ++i)
      SelfEncrypt(contents[i].get(), chunk_dir_, false, sep_, &(data_maps[i]));
    std::uint64_t duration =
        (boost::posix_time::microsec_clock::universal_time() -
         time).total_microseconds();
    if (duration == 0)
      duration = 1;
    printf("Self-encrypted %d strings à %d bytes in %.2f seconds "
           "(%.3f MB/s).\n", repetitions, data_size, duration / 1000000.0,
           (repetitions * data_size) / duration / 1.048576);

    std::vector<std::shared_ptr<std::ostringstream>> dec_contents;
    for (size_t i = 0; i < repetitions; ++i) {
      std::shared_ptr<std::ostringstream> stream_ptr(new std::ostringstream);
      dec_contents.push_back(stream_ptr);
    }
    ASSERT_EQ(repetitions, dec_contents.size());

    time = boost::posix_time::microsec_clock::universal_time();
    for (size_t i = 0; i < repetitions; ++i)
      SelfDecrypt(data_maps[i], chunk_dir_, dec_contents[i].get());
    duration = (boost::posix_time::microsec_clock::universal_time() -
                time).total_microseconds();
    if (duration == 0)
      duration = 1;

    printf("Self-decrypted %d strings à %d bytes in %.2f seconds "
           "(%.3f MB/s).\n", repetitions, data_size, duration / 1000000.0,
           (repetitions * data_size) / duration / 1.048576);

//     for (size_t i = 0; i < repetitions; ++i)
    EXPECT_EQ(contents[0]->str(), dec_contents[0]->str());

    fs::remove_all(chunk_dir_);
  }
}

INSTANTIATE_TEST_CASE_P(ChunkSize, SelfEncryptionBenchmarkTest, testing::Values(
    // Variation in chunk_size
    SelfEncryptionParams(1 << 8, 0, kMinChunks - 1),  // 256 Bytes
    SelfEncryptionParams(1 << 10, 0, kMinChunks - 1),  // 1 KiB
    SelfEncryptionParams(1 << 12, 0, kMinChunks - 1),  // 4 KiB
    SelfEncryptionParams(1 << 14, 0, kMinChunks - 1),  // 16 KiB
    SelfEncryptionParams(1 << 16, 0, kMinChunks - 1),  // 64 KiB
    SelfEncryptionParams(1 << 17, 0, kMinChunks - 1),  // 128 KiB
    SelfEncryptionParams(1 << 18, 0, kMinChunks - 1),  // 256 KiB (default)
    SelfEncryptionParams(1 << 19, 0, kMinChunks - 1),  // 512 KiB
    SelfEncryptionParams(1 << 20, 0, kMinChunks - 1),  // 1 MiB
    SelfEncryptionParams(1 << 21, 0, kMinChunks - 1)  // 2 MiB
));

INSTANTIATE_TEST_CASE_P(IncData, SelfEncryptionBenchmarkTest, testing::Values(
    // Variation in max_includable_data_size
    SelfEncryptionParams(1 << 18, 0, 1 << 6),  // 64 Bytes
    SelfEncryptionParams(1 << 18, 0, 1 << 8),  // 256 Bytes
    SelfEncryptionParams(1 << 18, 0, 1 << 10),  // 1 KiB (default)
    SelfEncryptionParams(1 << 18, 0, 1 << 12),  // 4 KiB
    SelfEncryptionParams(1 << 18, 0, 1 << 14),  // 16 KiB
    SelfEncryptionParams(1 << 18, 0, 1 << 16)  // 64 KiB
));

INSTANTIATE_TEST_CASE_P(IncChunk, SelfEncryptionBenchmarkTest, testing::Values(
    // Variation in max_includable_chunk_size
    SelfEncryptionParams(1 << 18, 1 << 6, 1 << 8),  // 64 Bytes
    SelfEncryptionParams(1 << 18, 1 << 8, 1 << 10),  // 256 Bytes (default)
    SelfEncryptionParams(1 << 18, 1 << 10, 1 << 12),  // 1 KiB
    SelfEncryptionParams(1 << 18, 1 << 12, 1 << 14),  // 4 KiB
    SelfEncryptionParams(1 << 18, 1 << 14, 1 << 16),  // 16 KiB
    SelfEncryptionParams(1 << 18, 1 << 16, 1 << 18)  // 64 KiB
));

}  // namespace encrypt

}  // namespace test

}  // namespace maidsafe
