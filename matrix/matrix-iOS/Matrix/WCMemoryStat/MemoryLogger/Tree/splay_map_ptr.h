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

#ifndef splay_map_ptr_h
#define splay_map_ptr_h

#include <functional>
#include <stack>
#include <assert.h>

#include "buffer_source.h"

template<typename TKey, typename TVal> class splay_map_ptr {
private:
    struct node;
    typedef struct node *node_ptr;

    struct node {
        TKey key;
        TVal val;
        node_ptr left, right;
    };

    node_ptr root_ptr = NULL;
    node_ptr last_ptr = NULL; // use for exist() and find()
    node_ptr free_ptr = NULL;
    uint32_t t_size; // tree size

    memory_pool_file *memory_pool;

    bool splay(TKey key, node_ptr &root) {
        /* Simple top down splay, not requiring i to be in the tree t.  */
        /* What it does is described above.                             */
        if (root == NULL) {
            return false;
        }

        node N;
        node_ptr l, r, t;
        N.left = N.right = NULL;
        l = r = &N;
        t = root;
        bool ret = false;

        for (;;) {
            if (t->key > key) {
                node_ptr tlc = t->left;
                if (tlc == NULL) {
                    break;
                }
                if (tlc->key > key) {
                    t->left = tlc->right; /* rotate right */
                    tlc->right = t;
                    t = tlc;
                    if (t->left == NULL) {
                        break;
                    }
                }
                r->left = t; /* link right */
                r = t;
                t = t->left;
            } else if (t->key < key) {
                node_ptr trc = t->right;
                if (trc == NULL) {
                    break;
                }
                if (trc->key < key) {
                    t->right = trc->left; /* rotate left */
                    trc->left = t;
                    t = trc;
                    if (t->right == NULL) {
                        break;
                    }
                }
                l->right = t; /* link left */
                l = t;
                t = t->right;
            } else {
                ret = true;
                break;
            }
        }
        l->right = t->left; /* assemble */
        r->left = t->right;
        t->left = N.right;
        t->right = N.left;
        root = t;
        return ret;
    }

    node_ptr inter_find(TKey key) {
        if (splay(key, root_ptr)) {
            return root_ptr;
        } else {
            return NULL;
        }
    }

    void inter_enumerate(node_ptr root, void (^callback)(const TKey &key, const TVal &val)) {
        if (root == NULL) {
            return;
        }

        // not support
    }

    void free_node(node_ptr ptr) {
        ptr->left = free_ptr;
        free_ptr = ptr;
    }

    node_ptr next_free_node() {
        node_ptr new_ptr = free_ptr;
        if (new_ptr == NULL) {
            int new_count = (4 << 10);
            node_ptr new_buffer = (node_ptr)memory_pool->malloc(new_count * sizeof(node));
            if (new_buffer == NULL) {
                return NULL;
            }

            for (int i = 1; i < new_count; ++i) {
                new_buffer[i - 1].left = &new_buffer[i];
            }
            new_buffer[new_count - 1].left = NULL;
            free_ptr = &new_buffer[1];
            new_ptr = &new_buffer[0];
        } else {
            free_ptr = free_ptr->left;
        }
        return new_ptr;
    }

public:
    splay_map_ptr(memory_pool_file *_pool) { memory_pool = _pool; }

    ~splay_map_ptr() {}

    void insert(TKey key, const TVal &val) {
        if (root_ptr == NULL) {
            root_ptr = next_free_node();
            if (root_ptr == NULL) {
                return;
            }

            t_size = 1;
            root_ptr->left = root_ptr->right = NULL;
            root_ptr->key = key;
            root_ptr->val = val;
            return;
        }

        if (splay(key, root_ptr)) {
            /* We get here if it's already in the tree */
            /* Don't add it again                      */
            root_ptr->val = val;
        } else if (root_ptr->key > key) {
            node_ptr n = next_free_node();
            if (n == NULL) {
                return;
            }

            ++t_size;
            n->key = key;
            n->val = val;
            n->left = root_ptr->left;
            n->right = root_ptr;
            root_ptr->left = NULL;
            root_ptr = n;
        } else {
            node_ptr n = next_free_node();
            if (n == NULL) {
                return;
            }

            ++t_size;
            n->key = key;
            n->val = val;
            n->right = root_ptr->right;
            n->left = root_ptr;
            root_ptr->right = NULL;
            root_ptr = n;
        }
    }

    void remove(TKey key) {
        /* Deletes i from the tree if it's there.               */
        /* Return a pointer to the resulting tree.              */
        if (splay(key, root_ptr)) { /* found it */
            node_ptr x;

            if (root_ptr->left == NULL) {
                x = root_ptr->right;
            } else {
                x = root_ptr->left;
                splay(key, x);
                x->right = root_ptr->right;
            }
            free_node(root_ptr);
            --t_size;
            root_ptr = x;
        }
    }

    bool exist(TKey key) { return (last_ptr = inter_find(key)) != NULL; }

    // should check exist() first
    TVal &find() { return last_ptr->val; }

    uint32_t size() { return t_size; }

    void enumerate(void (^callback)(const TKey &key, const TVal &val)) { inter_enumerate(root_ptr, callback); }
};

#endif /* splay_map_ptr_h */
