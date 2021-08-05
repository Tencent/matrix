/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef splay_map_h
#define splay_map_h

#include <functional>
#include <stack>
#include <assert.h>

#include "buffer_source.h"

template<typename TKey, typename TVal> class splay_map {
private:
    typedef uint32_t node_ptr;

    struct node {
        TKey key;
        node_ptr left, right;
    };

    struct tree_info {
        node_ptr root_ptr;
        node_ptr free_ptr;
        uint32_t b_size; // buffer size
        uint32_t t_size; // tree size
    };

    tree_info *t_info = NULL;
    tree_info t_empty = { 0 };
    node *k_buff = NULL; // node buffer
    node_ptr last_ptr = 0; // use for exist() and find()
    TVal *v_buff = NULL;
    uint32_t i_size = 0; // increment/init size

    buffer_source *key_buffer_source;
    buffer_source *val_buffer_source;

    buffer_source *self_key_buffer_source_ = nullptr;
    buffer_source *self_val_buffer_source_ = nullptr;

#define get_val(ptr) v_buff[ptr]

#define get_node(ptr) k_buff[ptr]

#define get_node_lc(ptr) k_buff[ptr].left

#define get_node_rc(ptr) k_buff[ptr].right

#define get_node_key(ptr) k_buff[ptr].key

    bool splay(TKey key, node_ptr &root) {
        /* Simple top down splay, not requiring i to be in the tree t.  */
        /* What it does is described above.                             */
        if (root == 0) {
            return false;
        }

        node_ptr l = 0, r = 0, t = root;
        bool ret = false;

        for (;;) {
            TKey tkey = get_node_key(t);
            if (tkey > key) {
                node_ptr tlc = get_node_lc(t);
                if (tlc == 0) {
                    break;
                }
                if (get_node_key(tlc) > key) {
                    get_node_lc(t) = get_node_rc(tlc); /* rotate right */
                    get_node_rc(tlc) = t;
                    t = tlc;
                    if (get_node_lc(t) == 0) {
                        break;
                    }
                }
                get_node_lc(r) = t; /* link right */
                r = t;
                t = get_node_lc(t);
            } else if (tkey < key) {
                node_ptr trc = get_node_rc(t);
                if (trc == 0) {
                    break;
                }
                if (get_node_key(trc) < key) {
                    get_node_rc(t) = get_node_lc(trc); /* rotate left */
                    get_node_lc(trc) = t;
                    t = trc;
                    if (get_node_rc(t) == 0) {
                        break;
                    }
                }
                get_node_rc(l) = t; /* link left */
                l = t;
                t = get_node_rc(t);
            } else {
                ret = true;
                break;
            }
        }
        get_node_rc(l) = get_node_lc(t); /* assemble */
        get_node_lc(r) = get_node_rc(t);
        get_node_lc(t) = get_node_rc(0);
        get_node_rc(t) = get_node_lc(0);
        get_node_lc(0) = 0;
        get_node_rc(0) = 0;
        root = t;
        return ret;
    }

    node_ptr inter_find(TKey key) {
        if (splay(key, t_info->root_ptr)) {
            return t_info->root_ptr;
        } else {
            return 0;
        }
    }

    void inter_enumerate(node_ptr root, std::function<void(const TKey &key, TVal &val)> callback) {
        if (root == 0) {
            return;
        }

        std::stack<node_ptr> s;
        uint32_t count = 0;
        while (root != 0 || !s.empty()) {
            while (root != 0) {
                s.push(root);
                if (s.size() > t_info->t_size) {
                    return;
                }
                root = get_node_lc(root);
            }

            if (!s.empty()) {
                root = s.top();
                callback(get_node_key(root), get_val(root));
                if (++count > t_info->t_size) {
                    return;
                }
                s.pop();
                root = get_node_rc(root);
            }
        }
    }

    void free_node(node_ptr ptr) {
        if (ptr == 0) {
            return;
        }
        get_node(ptr).left = t_info->free_ptr; // ticky
        t_info->free_ptr = ptr;
    }

    node_ptr next_free_node() {
        node_ptr next_ptr = t_info->free_ptr;
        if (next_ptr == 0) {
            next_ptr = t_info->t_size;
        } else {
            t_info->free_ptr = get_node(t_info->free_ptr).left; // ticky
        }
        return next_ptr;
    }

    bool reallocate_memory(bool is_init) {
        if (is_init) {
            // key buffer
            uint32_t malloc_size = sizeof(tree_info) + i_size * sizeof(node);
            void *new_buff = key_buffer_source->realloc(malloc_size);
            if (new_buff) {
                memset(new_buff, 0, malloc_size);
                t_info = (tree_info *)new_buff;
                k_buff = (node *)((char *)new_buff + sizeof(tree_info));
            } else {
                return false;
            }

            // val buffer
            malloc_size = i_size * sizeof(TVal);
            new_buff = val_buffer_source->realloc(malloc_size);
            if (new_buff) {
                memset(new_buff, 0, malloc_size);
                t_info->b_size = i_size;
                v_buff = (TVal *)new_buff;
                return true;
            } else {
                return false;
            }
        } else {
            t_empty = *t_info; // save t_info temporarily, t_info ptr will be invalid after reallocate new memory from file
            // key buffer
            uint32_t malloc_size = sizeof(tree_info) + (t_empty.b_size + i_size) * sizeof(node);
            void *new_buff = key_buffer_source->realloc(malloc_size);
            if (new_buff) {
                memset((char *)new_buff + sizeof(tree_info) + t_empty.b_size * sizeof(node), 0, i_size * sizeof(node));
                t_info = (tree_info *)new_buff;
                *t_info = t_empty;
                k_buff = (node *)((char *)new_buff + sizeof(tree_info));
            } else {
                return false;
            }

            // val buffer
            malloc_size = (t_empty.b_size + i_size) * sizeof(TVal);
            new_buff = val_buffer_source->realloc(malloc_size);
            if (new_buff) {
                memset((char *)new_buff + t_empty.b_size * sizeof(TVal), 0, i_size * sizeof(TVal));
                t_info->b_size = t_info->b_size + i_size;
                v_buff = (TVal *)new_buff;
                return true;
            } else {
                return false;
            }
        }
    }

public:
    splay_map(uint32_t _is) {
        self_key_buffer_source_ = new buffer_source_memory;
        self_val_buffer_source_ = new buffer_source_memory;
        _init(_is, self_key_buffer_source_, self_val_buffer_source_);
    }

    splay_map(uint32_t _is, buffer_source *_kb, buffer_source *_vb) {
        _init(_is, _kb, _vb);
    }

    void _init(uint32_t _is, buffer_source *_kb, buffer_source *_vb) {
        key_buffer_source = _kb;
        val_buffer_source = _vb;
        i_size = (_is == 0 ? 1024 : _is);

        void *data = key_buffer_source->buffer();
        size_t len = key_buffer_source->buffer_size();

        if (data != NULL && len > sizeof(tree_info)) {
            t_info = (tree_info *)data;
            // check valid
            if (t_info->b_size * sizeof(node) > len - sizeof(tree_info) || t_info->root_ptr >= t_info->b_size || t_info->free_ptr >= t_info->b_size
                || t_info->t_size >= t_info->b_size || val_buffer_source->buffer() == NULL
                || val_buffer_source->buffer_size() < t_info->b_size * sizeof(TVal)) {
                reallocate_memory(true);
            } else {
                k_buff = (node *)((char *)data + sizeof(tree_info));
                v_buff = (TVal *)val_buffer_source->buffer();
            }
        } else {
            reallocate_memory(true);
        }
    }

    ~splay_map() {
        if (self_key_buffer_source_) delete self_key_buffer_source_;
        if (self_val_buffer_source_) delete self_val_buffer_source_;
    }

    TVal *insert(TKey key, const TVal &val) {
        if (t_info->t_size + 1 == t_info->b_size && !reallocate_memory(false)) {
            return nullptr; // malloc fail
        }

        if (t_info->root_ptr == 0) {
            t_info->t_size = 1;

            node_ptr n = next_free_node();
            get_node_key(n) = key;
            get_node_lc(n) = 0;
            get_node_rc(n) = 0;
            get_val(n) = val;
            t_info->root_ptr = n;
            return &get_val(n);
        }

        if (splay(key, t_info->root_ptr)) {
            /* We get here if it's already in the tree */
            /* Don't add it again                      */
            get_val(t_info->root_ptr) = val;
            return &get_val(t_info->root_ptr);
        }

        node_ptr root_ptr = t_info->root_ptr;
        if (get_node_key(root_ptr) > key) {
            ++t_info->t_size;

            node_ptr n = next_free_node();
            get_node_key(n) = key;
            get_node_lc(n) = get_node_lc(root_ptr);
            get_node_rc(n) = root_ptr;
            get_val(n) = val;

            get_node_lc(root_ptr) = 0;
            t_info->root_ptr = n;

            return &get_val(n);
        } else {
            ++t_info->t_size;

            node_ptr n = next_free_node();
            get_node_key(n) = key;
            get_node_rc(n) = get_node_rc(root_ptr);
            get_node_lc(n) = root_ptr;
            get_val(n) = val;

            get_node_rc(root_ptr) = 0;
            t_info->root_ptr = n;

            return &get_val(n);
        }
    }

    TVal* remove(TKey key) {
        /* Deletes i from the tree if it's there.               */
        /* Return a pointer to the resulting tree.              */
        if (splay(key, t_info->root_ptr)) { /* found it */
            node_ptr x;

            if (get_node_lc(t_info->root_ptr) == 0) {
                x = get_node_rc(t_info->root_ptr);
            } else {
                x = get_node_lc(t_info->root_ptr);
                splay(key, x);
                get_node_rc(x) = get_node_rc(t_info->root_ptr);
            }
            node_ptr r = t_info->root_ptr;
            free_node(t_info->root_ptr);
            --t_info->t_size;
            t_info->root_ptr = x;
            return &get_val(r);
        }

        return nullptr;
    }

    bool exist(TKey key) { return (last_ptr = inter_find(key)) != 0; }

    // should check exist() first
    TVal &find() { return get_val(last_ptr); }

    uint32_t size() { return t_info->t_size; }

    void enumerate(std::function<void(const TKey &key, TVal &val)> callback) { inter_enumerate(t_info->root_ptr, callback); }
};

#endif /* splay_map_h */
