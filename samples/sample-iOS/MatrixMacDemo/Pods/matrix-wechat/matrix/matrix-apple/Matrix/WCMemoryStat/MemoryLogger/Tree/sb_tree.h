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

#ifndef sb_tree_h
#define sb_tree_h

#include <functional>
#include <assert.h>

typedef void *(*sbt_reallocator)(void *context, void *oldMem, size_t newSize);
typedef void  (*sbt_deallocator)(void *context, void *mem);

template <typename T>
class sb_tree {
private:
	typedef uint32_t node_ptr;
	
	struct node {
		T key;
		node_ptr left, right;
		uint32_t size; // todo
		node(const T &init = T()) : left(0), right(0), key(init), size(1) {}
	};
	
	struct tree_info {
		node_ptr root_ptr;
		node_ptr free_ptr;
		uint32_t b_size; // buffer size
		uint32_t t_size; // tree size
	};
	
	tree_info		*t_info = NULL;
	tree_info		t_empty = {0};
	node			*n_buff = NULL; // node buffer
	node_ptr		last_ptr = 0; // use for exist() and find()
	uint32_t		i_size = 0; // increment/init size
	sbt_reallocator reallocator = NULL;
	sbt_deallocator	deallocator = NULL;
	void			*context = NULL;
	
	inline node &get_node(node_ptr ptr) {
		return n_buff[ptr];
	}
	
	inline node_ptr &get_node_lc(node_ptr ptr) {
		return n_buff[ptr].left;
	}
	
	inline node_ptr &get_node_rc(node_ptr ptr) {
		return n_buff[ptr].right;
	}
	
	inline uint32_t &get_node_size(node_ptr ptr) {
		return n_buff[ptr].size;
	}
	
	inline T &get_node_key(node_ptr ptr) {
		return n_buff[ptr].key;
	}
	
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
		if (root == 0) {
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
	
	void inter_insert(node_ptr &root, const T &key) {
		if (root == 0) {
			root = next_free_node();
			get_node(root) = node(key);
		} else {
			++get_node_size(root);
			if (get_node_key(root) > key) {
				inter_insert(get_node_lc(root), key);
				left_balance(root);
			} else {
				inter_insert(get_node_rc(root), key);
				right_balance(root);
			}
		}
	}
//	void inter_insert(node_ptr &root, const T &key) {
//		int level = 0;
//		node_ptr stack[128];
//		node_ptr now = root;
//		bool child[128];
//		while (now) {
//			stack[level] = now;
//			++get_node_size(now);
//			child[level] = (get_node_key(now) > key);
//			if (child[level]) {
//				now = get_node_lc(now);
//			} else {
//				now = get_node_rc(now);
//			}
//			++level;
//			assert(level < 128);
//		}
//		
//		now = next_free_node();
//		get_node(now) = node(key);
//		while (level--) {
//			node_ptr parent = stack[level];
//			if (child[level]) {
//				get_node_lc(parent) = now;
//				maintain(parent, false);
//			} else {
//				get_node_rc(parent) = now;
//				maintain(parent, true);
//			}
//			now = parent;
//		}
//		root = now;
//	}
	
	node_ptr find_min(node_ptr root) {
		node_ptr lc;
		while ((lc = get_node_lc(root))) {
			root = lc;
		}
		return root;
	}
	
	node_ptr find_max(node_ptr root) {
		node_ptr rc;
		while ((rc = get_node_rc(root))) {
			root = rc;
		}
		return root;
	}
	
	bool inter_remove(node_ptr &root, const T &key) {
		if (root == 0) {
			return false;
		}
		
		bool ret = true;
		node_ptr tmp = root;
		if (get_node_key(root) > key) {
			ret = inter_remove(get_node_lc(root), key);
		} else if (get_node_key(root) < key) {
			ret = inter_remove(get_node_rc(root), key);
		} else {
			node_ptr &lc = get_node_lc(root);
			node_ptr &rc = get_node_rc(root);
			if (lc != 0 && rc != 0) {
				if (get_node_size(lc) > get_node_size(rc)) {
					get_node_key(root) = get_node_key(find_max(lc));
					inter_remove(lc, get_node_key(root));
				} else {
					get_node_key(root) = get_node_key(find_min(rc));
					inter_remove(rc, get_node_key(root));
				}
			} else {
				root = (lc != 0 ? lc : rc);
				free_node(tmp);
			}
		}
		if (ret) {
			--get_node_size(tmp);
		}
		return ret;
	}
	
	node_ptr inter_find(node_ptr root, const T &key) {
		while ((get_node_key(root) != key) && (root = (get_node_key(root) > key ? get_node_lc(root) : get_node_rc(root))));
		return root;
	}
	
	bool reallocate_memory(bool is_init) {
		if (reallocator) {
			return reallocate_memory_from_file(is_init);
		} else {
			return reallocate_memory_from_system(is_init);
		}
	}
	
	bool reallocate_memory_from_system(bool is_init) {
		if (is_init) {
			t_info = &t_empty;
		}
		
		uint32_t malloc_size = (t_info->b_size + i_size) * sizeof(node);
		void *new_buff = inter_realloc(n_buff, malloc_size);
		if (new_buff) {
			memset((char *)new_buff + t_info->b_size * sizeof(node), 0, i_size * sizeof(node));
			t_info->b_size = t_info->b_size + i_size;
			n_buff = (node *)new_buff;
			return true;
		} else {
			return false;
		}
	}
	
	bool reallocate_memory_from_file(bool is_init) {
		if (is_init) {
			uint32_t malloc_size = sizeof(tree_info) + i_size * sizeof(node);
			void *new_buff = reallocator(context, NULL, malloc_size);
			if (new_buff) {
				memset(new_buff, 0, malloc_size);
				t_info = (tree_info *)new_buff;
				t_info->b_size = i_size;
				n_buff = (node *)((char *)new_buff + sizeof(tree_info));
				return true;
			} else {
				return false;
			}
		} else {
			t_empty = *t_info; // save t_info temporarily, t_info ptr will be invalid after reallocate new memory from file
			uint32_t malloc_size = sizeof(tree_info) + (t_info->b_size + i_size) * sizeof(node);
			void *new_buff = reallocator(context, t_info, malloc_size);
			if (new_buff) {
				memset((char *)new_buff + sizeof(tree_info) + t_empty.b_size * sizeof(node), 0, i_size * sizeof(node));
				t_info = (tree_info *)new_buff;
				*t_info = t_empty;
				t_info->b_size = t_info->b_size + i_size;
				n_buff = (node *)((char *)new_buff + sizeof(tree_info));
				return true;
			} else {
				return false;
			}
		}
	}
	
	bool check_tree(node_ptr root) {
		if (root == 0) {
			return true;
		}
		if (!check_tree(get_node_lc(root))) {
			return false;
		}
		if (!check_tree(get_node_rc(root))) {
			return false;
		}
		if (get_node_size(root) != get_node_size(get_node_lc(root)) + get_node_size(get_node_rc(root)) + 1) {
			return false;
		}
		return true;
	}
	
public:
	sb_tree(uint32_t _is=1024) {
		i_size = (_is == 0 ? 1024 : _is);
		reallocate_memory(true);
	}
	
	sb_tree(uint32_t _is, sbt_reallocator _a, sbt_deallocator _d, void *_data=NULL, size_t _len=0, void *_context=NULL) : reallocator(_a), deallocator(_d), context(_context) {
		assert(reallocator != NULL && deallocator != NULL);
		i_size = (_is == 0 ? 1024 : _is);
		if (_data != NULL && _len > sizeof(tree_info)) {
			t_info = (tree_info *)_data;
			// check valid
			if (t_info->b_size * sizeof(node) > _len - sizeof(tree_info) ||
				t_info->root_ptr >= t_info->b_size ||
				t_info->free_ptr >= t_info->b_size ||
				t_info->t_size >= t_info->b_size) {
				reallocate_memory(true);
			} else {
				n_buff = (node *)((char *)_data + sizeof(tree_info));
				if (!check_tree(t_info->root_ptr)) {
					reallocate_memory(true);
				}
			}
		} else {
			reallocate_memory(true);
		}
	}
	
	~sb_tree() {
		if (deallocator) {
			deallocator(context, t_info);
		} else {
			inter_free(n_buff);
		}
	}
	
	bool exist(const T &key) {
		return (last_ptr = inter_find(t_info->root_ptr, key)) != 0;
	}
	
	void insert(const T &key) {
		if ((!t_info->b_size || t_info->t_size == t_info->b_size - 1) && reallocate_memory(false) == false) {
			return; // malloc fail
		}
		++t_info->t_size;
		inter_insert(t_info->root_ptr, key);
	}
	
	void remove(const T &key) {
		bool ret = inter_remove(t_info->root_ptr, key);
		t_info->t_size -= (ret ? 1 : 0);
	}
	
	// should check exist() first
	T &find() {
		return get_node_key(last_ptr);
	}
	
	uint32_t size() {
		return t_info->t_size;
	}
	
	// should check exist() first
	uint32_t foundIndex() {
		return last_ptr;
	}
};

#endif /* sb_tree_h */
