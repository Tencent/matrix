/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <procinfo/process_map.h>

#include <string>

#include <android-base/file.h>

#include <gtest/gtest.h>

TEST(process_map, ReadMapFile) {
  std::string map_file = android::base::GetExecutableDirectory() + "/testdata/maps";
  std::vector<android::procinfo::MapInfo> maps;
  ASSERT_TRUE(android::procinfo::ReadMapFile(
      map_file,
      [&](uint64_t start, uint64_t end, uint16_t flags, uint64_t pgoff, ino_t inode,
          const char* name) { maps.emplace_back(start, end, flags, pgoff, inode, name); }));
  ASSERT_EQ(2043u, maps.size());
  ASSERT_EQ(maps[0].start, 0x12c00000ULL);
  ASSERT_EQ(maps[0].end, 0x2ac00000ULL);
  ASSERT_EQ(maps[0].flags, PROT_READ | PROT_WRITE);
  ASSERT_EQ(maps[0].pgoff, 0ULL);
  ASSERT_EQ(maps[0].inode, 10267643UL);
  ASSERT_EQ(maps[0].name, "[anon:dalvik-main space (region space)]");
  ASSERT_EQ(maps[876].start, 0x70e6c4f000ULL);
  ASSERT_EQ(maps[876].end, 0x70e6c6b000ULL);
  ASSERT_EQ(maps[876].flags, PROT_READ | PROT_EXEC);
  ASSERT_EQ(maps[876].pgoff, 0ULL);
  ASSERT_EQ(maps[876].inode, 2407UL);
  ASSERT_EQ(maps[876].name, "/system/lib64/libutils.so");
  ASSERT_EQ(maps[1260].start, 0x70e96fa000ULL);
  ASSERT_EQ(maps[1260].end, 0x70e96fb000ULL);
  ASSERT_EQ(maps[1260].flags, PROT_READ);
  ASSERT_EQ(maps[1260].pgoff, 0ULL);
  ASSERT_EQ(maps[1260].inode, 10266154UL);
  ASSERT_EQ(maps[1260].name,
            "[anon:dalvik-classes.dex extracted in memory from "
            "/data/app/com.google.sample.tunnel-HGGRU03Gu1Mwkf_-RnFmvw==/base.apk]");
}

TEST(process_map, ReadProcessMaps) {
  std::vector<android::procinfo::MapInfo> maps;
  ASSERT_TRUE(android::procinfo::ReadProcessMaps(
      getpid(),
      [&](uint64_t start, uint64_t end, uint16_t flags, uint64_t pgoff, ino_t inode,
          const char* name) { maps.emplace_back(start, end, flags, pgoff, inode, name); }));
  ASSERT_GT(maps.size(), 0u);
  maps.clear();
  ASSERT_TRUE(android::procinfo::ReadProcessMaps(getpid(), &maps));
  ASSERT_GT(maps.size(), 0u);
}
