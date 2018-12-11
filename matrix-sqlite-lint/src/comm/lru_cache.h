/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Created by yanguoyue on 2018/1/17.
//

#ifndef MATRIX_ANDROID_LRU_CACHE_H
#define MATRIX_ANDROID_LRU_CACHE_H

#include <stddef.h>
#include <list>
#include <unordered_map>


namespace sqlitelint {

    template <typename K, typename V> class LRUCache {
    public:
        LRUCache(size_t _max_size): max_size_(_max_size) {}

        void Put(const K& _key, const V& _val) {

            auto iter = cache_map_.find(_key);
            if (iter != cache_map_.end()) {
                keys_lst_.erase(iter->second.second);
                keys_lst_.push_front(_key);
                cache_map_[_key] = std::pair<ValType, typename std::list<KeyType>::iterator>(_val, keys_lst_.begin());
                return;
            }

            keys_lst_.push_front(_key);
            cache_map_[_key] = std::pair<ValType, typename std::list<KeyType>::iterator>(_val, keys_lst_.begin());

            if (keys_lst_.size() > max_size_) {
                auto last = keys_lst_.end();
                last --;
                cache_map_.erase(*last);
                keys_lst_.erase(last);
            }

        }

        bool Get(const K& _key, V& _val) {

            auto iter = cache_map_.find(_key);
            if (cache_map_.end() == iter) {
                return false;
            }

            _val = iter->second.first;
            keys_lst_.splice(keys_lst_.begin(), keys_lst_, iter->second.second);
            return true;

        }

        bool Exists(const K& _key) {
            return cache_map_.find(_key) != cache_map_.end();
        }

        void Clear() {
            keys_lst_.clear();
            cache_map_.clear();
        }

        size_t Size() {
            return keys_lst_.size();
        }

        bool Empty() {
            return keys_lst_.empty();
        }

    private:
        typedef K KeyType;
        typedef V ValType;

        std::list<KeyType> keys_lst_;
        std::unordered_map<KeyType, std::pair<ValType, typename std::list<KeyType>::iterator> > cache_map_;
        size_t max_size_;
    };

}

#endif //MATRIX_ANDROID_LRU_CACHE_H
