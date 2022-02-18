/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef stack_hash_map_h
#define stack_hash_map_h

#include "buffer_source.h"

typedef uint64_t sh_map_key_t;
typedef uint32_t sh_map_val_t;

class sh_map_node_slot {
public:
    typedef uint16_t node_index;

    sh_map_key_t key;
    sh_map_val_t val;
    node_index next_index;

    sh_map_node_slot(sh_map_key_t _key, sh_map_val_t _val, node_index _ni) {
        key = _key;
        val = _val;
        next_index = _ni;
    };

public:
    static sh_map_node_slot *alloc(sh_map_key_t _key, sh_map_val_t _val) {
        sh_map_node_slot *node = (sh_map_node_slot *)inter_malloc(sizeof(sh_map_node_slot));
        node->key = _key;
        node->val = _val;
        return node;
    }

    static void release(void *node) { inter_free(node); }

    bool exist(sh_map_key_t _key) { return key == _key; }

    sh_map_val_t find() { return val; }
};

class sh_map_node_tree {
private:
    typedef uint32_t node_ptr;

    struct node {
        sh_map_key_t key;
        sh_map_val_t val;
        uint32_t size; // todo
        node_ptr lc;
        node_ptr rc;
        node(sh_map_key_t _key, sh_map_val_t _val, node_ptr _null_ptr) : key(_key), val(_val), size(1), lc(_null_ptr), rc(_null_ptr) {}
    };
    static_assert(sizeof(node) % 8 == 0, "Not aligned!");

    node_ptr root_ptr;
    node_ptr free_ptr;
    node_ptr null_ptr;
    node_ptr last_ptr; // use for exist() and find()

#define get_node(ptr) ((node *)(((uintptr_t)(ptr)) << 3))

#define to_node_ptr(orig_ptr) (node_ptr)(((uintptr_t)(orig_ptr)) >> 3)

#define get_node_lc(ptr) get_node(ptr)->lc

#define get_node_rc(ptr) get_node(ptr)->rc

#define get_node_size(ptr) get_node(ptr)->size

#define get_node_key(ptr) get_node(ptr)->key

#define get_node_val(ptr) get_node(ptr)->val

    inline void left_rotate(node_ptr &root) {
        node_ptr rc = get_node_rc(root);
        get_node_rc(root) = get_node_lc(rc);
        get_node_lc(rc) = root;
        get_node_size(rc) = get_node_size(root);
        get_node_size(root) = get_node_size(get_node_lc(root)) + get_node_size(get_node_rc(root)) + 1;
        root = rc;
    }

    inline void right_rotate(node_ptr &root) {
        node_ptr lc = get_node_lc(root);
        get_node_lc(root) = get_node_rc(lc);
        get_node_rc(lc) = root;
        get_node_size(lc) = get_node_size(root);
        get_node_size(root) = get_node_size(get_node_lc(root)) + get_node_size(get_node_rc(root)) + 1;
        root = lc;
    }

    inline void left_balance(node_ptr &root) {
        uint32_t rc_size = get_node_size(get_node_rc(root));
        if (get_node_size(get_node_lc(get_node_lc(root))) > rc_size) {
            right_rotate(root);
        } else if (get_node_size(get_node_rc(get_node_lc(root))) > rc_size) {
            left_rotate(get_node_lc(root));
            right_rotate(root);
        }
    }

    inline void right_balance(node_ptr &root) {
        uint32_t lc_size = get_node_size(get_node_lc(root));
        if (get_node_size(get_node_rc(get_node_rc(root))) > lc_size) {
            left_rotate(root);
        } else if (get_node_size(get_node_lc(get_node_rc(root))) > lc_size) {
            right_rotate(get_node_rc(root));
            left_rotate(root);
        }
    }

    void maintain(node_ptr &root, bool flag) {
        if (root == null_ptr) {
            return;
        }

        if (flag == false) {
            uint32_t rc_size = get_node_size(get_node_rc(root));
            if (get_node_size(get_node_lc(get_node_lc(root))) > rc_size) {
                right_rotate(root);
            } else if (get_node_size(get_node_rc(get_node_lc(root))) > rc_size) {
                left_rotate(get_node_lc(root));
                right_rotate(root);
            } else {
                return;
            }
        } else {
            uint32_t lc_size = get_node_size(get_node_lc(root));
            if (get_node_size(get_node_rc(get_node_rc(root))) > lc_size) {
                left_rotate(root);
            } else if (get_node_size(get_node_lc(get_node_rc(root))) > lc_size) {
                right_rotate(get_node_rc(root));
                left_rotate(root);
            } else {
                return;
            }
        }
        maintain(get_node_lc(root), false);
        maintain(get_node_rc(root), true);
        maintain(root, true);
        maintain(root, false);
    }

    node_ptr next_free_node() {
        node_ptr next_ptr = free_ptr;
        if (next_ptr == 0) {
            int new_count = 8;
            node *new_buffer = (node *)shared_memory_pool_file_alloc(new_count * sizeof(node));
            if (new_buffer == NULL) {
                return null_ptr;
            }

            for (int i = 2; i < new_count; ++i) {
                new_buffer[i - 1].lc = to_node_ptr(&new_buffer[i]);
            }
            new_buffer[new_count - 1].lc = 0;
            free_ptr = to_node_ptr(&new_buffer[1]);
            next_ptr = to_node_ptr(&new_buffer[0]);
        } else {
            free_ptr = get_node_lc(free_ptr);
        }
        return next_ptr;
    }

    void inter_insert(node_ptr &root, sh_map_key_t key, sh_map_val_t val) {
        if (root == null_ptr) {
            root = next_free_node();
            *get_node(root) = node(key, val, null_ptr);
        } else {
            ++get_node_size(root);
            if (get_node_key(root) > key) {
                inter_insert(get_node_lc(root), key, val);
                left_balance(root);
            } else {
                inter_insert(get_node_rc(root), key, val);
                right_balance(root);
            }
        }
    }

    node_ptr inter_find(node_ptr root, sh_map_key_t key) {
        while (root != null_ptr && (get_node_key(root) != key)) {
            root = (get_node_key(root) > key ? get_node_lc(root) : get_node_rc(root));
        }
        return root;
    }

public:
    static sh_map_node_tree *alloc() {
        sh_map_node_tree *tree = (sh_map_node_tree *)inter_calloc(1, sizeof(sh_map_node_tree));
        tree->init();
        return tree;
    }

    void init() {
        root_ptr = null_ptr = next_free_node();
        get_node_lc(null_ptr) = get_node_rc(null_ptr) = null_ptr;
        get_node_key(null_ptr) = 0;
        get_node_size(null_ptr) = 0;
    }

    bool exist(sh_map_key_t key) { return (last_ptr = inter_find(root_ptr, key)) != null_ptr; }

    sh_map_val_t find() { return get_node_val(last_ptr); }

    void insert(sh_map_key_t key, sh_map_val_t val) { inter_insert(root_ptr, key, val); }

    int stat() { return get_node_size(root_ptr); }

#undef to_node_ptr
#undef get_node
#undef get_node_lc
#undef get_node_rc
#undef get_node_size
#undef get_node_key
#undef get_node_val
};

class sh_map_node_trunk {
private:
    typedef uint16_t node_index;

#define BUFF_SIZE 4
#define NOT_FOUND UINT16_MAX

    node_index head_index = 0;
    node_index tail_index = 0;
    node_index free_index = 0;
    node_index last_index = 0;
    sh_map_node_tree *tree_ptr;
    sh_map_node_slot n_buff[BUFF_SIZE];

private:
    inline node_index next_free_node() { return free_index++; }

    node_index inter_find(sh_map_key_t key) {
        if (head_index != NOT_FOUND) {
            sh_map_node_slot &node = n_buff[head_index];
            if (node.key == key) {
                return head_index;
            }

            node_index prev_index = head_index;
            node_index curr_index = node.next_index;
            while (curr_index != NOT_FOUND) {
                sh_map_node_slot &node = n_buff[curr_index];
                if (node.key == key) {
                    break;
                }
                prev_index = curr_index;
                curr_index = node.next_index;
            }

            if (curr_index != NOT_FOUND) {
                n_buff[prev_index].next_index = n_buff[curr_index].next_index;
                n_buff[curr_index].next_index = head_index;
                head_index = curr_index;
                if (curr_index == tail_index) {
                    assert(n_buff[prev_index].next_index == NOT_FOUND);
                    tail_index = prev_index;
                }
                return curr_index;
            } else if (tree_ptr != NULL) {
                if (tree_ptr->exist(key)) {
                    sh_map_val_t val = tree_ptr->find();
                    n_buff[tail_index] = sh_map_node_slot(key, val, head_index);
                    head_index = tail_index;
                    for (int i = 0; i < BUFF_SIZE; ++i) {
                        if (n_buff[i].next_index == tail_index) {
                            tail_index = i;
                            n_buff[i].next_index = NOT_FOUND;
                            break;
                        }
                    }
                    return head_index;
                }
            }
            return NOT_FOUND;
        } else {
            return NOT_FOUND;
        }
    }

public:
    static sh_map_node_trunk *alloc() {
        sh_map_node_trunk *node = (sh_map_node_trunk *)inter_malloc(sizeof(sh_map_node_trunk));
        node->head_index = NOT_FOUND;
        node->tail_index = 0;
        node->free_index = 0;
        node->tree_ptr = NULL;
        return node;
    }

    bool exist(sh_map_key_t key) {
        last_index = inter_find(key);
        return last_index != NOT_FOUND;
    }

    sh_map_val_t find() { return n_buff[last_index].val; }

    void insert(sh_map_key_t key, sh_map_val_t val) {
        if (free_index == BUFF_SIZE) {
            if (tree_ptr == NULL) {
                tree_ptr = sh_map_node_tree::alloc();
                // 把之前结点插进去
                for (int i = 0; i < BUFF_SIZE; ++i) {
                    tree_ptr->insert(n_buff[i].key, n_buff[i].val);
                }
            }
            tree_ptr->insert(key, val);
            // 调整 lru
            n_buff[tail_index] = sh_map_node_slot(key, val, head_index);
            head_index = tail_index;
            for (int i = 0; i < BUFF_SIZE; ++i) {
                if (n_buff[i].next_index == tail_index) {
                    tail_index = i;
                    n_buff[i].next_index = NOT_FOUND;
                    break;
                }
            }
        } else {
            node_index new_ptr = next_free_node();
            sh_map_node_slot &new_node = n_buff[new_ptr];
            new_node.key = key;
            new_node.val = val;
            new_node.next_index = head_index;
            head_index = new_ptr;
        }
    }

    int stat() {
        if (free_index == BUFF_SIZE) {
            if (tree_ptr) {
                return tree_ptr->stat();
            } else {
                return BUFF_SIZE;
            }
        } else {
            return free_index;
        }
    }

#undef BUFF_SIZE
#undef NOT_FOUND
};

class stack_hash_map {
private:
    struct sh_map_node {
        union {
            void *ptr;

            struct {
                uint64_t orig_ptr : 56;
                uint64_t type : 8; // 1=slot, 0=trunk
            } detail;
        };
    };

    sh_map_node *node_list;
    uint32_t list_size;
    uint32_t last_index;

public:
    static stack_hash_map *alloc(uint32_t ls) {
        stack_hash_map *map = (stack_hash_map *)inter_malloc(sizeof(stack_hash_map));
        map->list_size = ls;
        map->node_list = (sh_map_node *)inter_calloc(ls, sizeof(sh_map_node));
        return map;
    }

    static void release(stack_hash_map *map) {
        if (map == NULL) {
            return;
        }

        inter_free(map->node_list);
        inter_free(map);
    }

    bool exist(sh_map_key_t key) {
        last_index = key % list_size;
        sh_map_node node = node_list[last_index];

        if (node.ptr != NULL) {
            switch (node.detail.type) {
                case 1:
                    return ((sh_map_node_slot *)node.detail.orig_ptr)->exist(key);
                case 0:
                    return ((sh_map_node_trunk *)node.ptr)->exist(key);
                default:
                    abort();
            }
            return false;
        } else {
            return false;
        }
    }

    sh_map_val_t find() {
        sh_map_node node = node_list[last_index];
        switch (node.detail.type) {
            case 1:
                return ((sh_map_node_slot *)node.detail.orig_ptr)->find();
            case 0:
                return ((sh_map_node_trunk *)node.ptr)->find();
            default:
                abort();
        }
        return 0;
    }

    void insert(sh_map_key_t key, sh_map_val_t val) {
        uint32_t index = key % list_size;
        sh_map_node node = node_list[index];

        if (node.ptr == NULL) {
            sh_map_node_slot *new_node = sh_map_node_slot::alloc(key, val);
            node_list[index] = { .detail.type = 1, .detail.orig_ptr = (uintptr_t)new_node };
        } else {
            switch (node.detail.type) {
                case 1: {
                    sh_map_node_slot *old_node = (sh_map_node_slot *)node.detail.orig_ptr;
                    sh_map_node_trunk *new_node = sh_map_node_trunk::alloc();
                    new_node->insert(old_node->key, old_node->val);
                    new_node->insert(key, val);
                    node_list[index].ptr = (void *)(uintptr_t)new_node;
                    sh_map_node_slot::release(old_node);
                    break;
                }
                case 0: {
                    sh_map_node_trunk *old_node = (sh_map_node_trunk *)node.ptr;
                    old_node->insert(key, val);
                    break;
                }
            }
        }
    }

    void stat() {
        int slot_count = 0;
        int trunk_count = 0;
        int empty_count = 0;

        int node_in_slot = 0;
        int node_in_trunk = 0;
        int node_in_trunk_max = 0;

        for (int i = 0; i < list_size; ++i) {
            sh_map_node node = node_list[i];
            if (node.ptr == NULL) {
                empty_count++;
            } else if (node.detail.type == 0) {
                trunk_count++;
                int stat = ((sh_map_node_trunk *)node.ptr)->stat();
                node_in_trunk_max = MAX(stat, node_in_trunk_max);
                node_in_trunk += stat;
            } else {
                slot_count++;
                node_in_slot++;
            }
        }

        char buff[2][1024];
        sprintf(buff[0], "slot: %d, trunk: %d, empty: %d", slot_count, trunk_count, empty_count);
        sprintf(buff[1], "node_in_slot: %d, node_in_trunk: %d, node_in_trunk_max: %d", node_in_slot, node_in_trunk, node_in_trunk_max);
        puts(buff[0]);
        puts(buff[1]);
    }
};

#endif /* stack_hash_map_h */
