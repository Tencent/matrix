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

#ifndef splay_tree_h
#define splay_tree_h

#include <functional>
#include <stack>
#include <assert.h>

typedef void *(*spt_reallocator)(void *context, void *oldMem, size_t newSize);
typedef void  (*spt_deallocator)(void *context, void *mem);

template <typename T>
class splay_tree {
private:
	typedef uint32_t node_ptr;
	
	struct node {
		node_ptr left, right;
		T key;
		node(const T &init) : left(0), right(0), key(init) {}
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
	spt_reallocator	reallocator = NULL;
	spt_deallocator	deallocator = NULL;
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
	
	inline T &get_node_key(node_ptr ptr) {
		return n_buff[ptr].key;
	}
	
	bool splay(const T &key, node_ptr &root) {
		/* Simple top down splay, not requiring i to be in the tree t.  */
		/* What it does is described above.							 */
		if (root == 0) return false;

		node_ptr y, l = 0, r = 0, t = root;
		bool ret = false;
		
		for (;;) {
			if (get_node_key(t) > key) {
				if (get_node_lc(t) == 0) break;
				if (get_node_key(get_node_lc(t)) > key) {
					y = get_node_lc(t);						   /* rotate right */
					get_node_lc(t) = get_node_rc(y);
					get_node_rc(y) = t;
					t = y;
					if (get_node_lc(t) == 0) break;
				}
				get_node_lc(r) = t;							   /* link right */
				r = t;
				t = get_node_lc(t);
			} else if (get_node_key(t) < key) {
				if (get_node_rc(t) == 0) break;
				if (get_node_key(get_node_rc(t)) < key) {
					y = get_node_rc(t);						  /* rotate left */
					get_node_rc(t) = get_node_lc(y);
					get_node_lc(y) = t;
					t = y;
					if (get_node_rc(t) == 0) break;
				}
				get_node_rc(l) = t;							  /* link left */
				l = t;
				t = get_node_rc(t);
			} else {
				ret = true;
				break;
			}
		}
		get_node_rc(l) = get_node_lc(t);								/* assemble */
		get_node_lc(r) = get_node_rc(t);
		get_node_lc(t) = get_node_rc(0);
		get_node_rc(t) = get_node_lc(0);
		get_node_lc(0) = 0;
		get_node_rc(0) = 0;
		root = t;
		return ret;
	}
	
	node_ptr inter_find(const T &key) {
		if (splay(key, t_info->root_ptr)) {
			return t_info->root_ptr;
		} else {
			return 0;
		}
	}
	
	void inter_enumerate(node_ptr root, void (^callback)(const T& key)) {
		if (root == 0) return;
		
		std::stack<node_ptr> s;
		uint32_t count = 0;
		while (root != 0 || s.empty() == false) {
			while (root != 0) {
				s.push(root);
				if (s.size() > t_info->t_size) {
					abort();
				}
				root = get_node_lc(root);
			}
			
			if (s.empty() == false) {
				root = s.top();
				callback(get_node_key(root));
				if (++count > t_info->t_size) {
					abort();
				}
				s.pop();
				root = get_node_rc(root);
			}
		}
	}
	
	void free_node(node_ptr ptr) {
		if (ptr == 0) return;
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
	
public:
	splay_tree(uint32_t _is=1024) {
		i_size = (_is == 0 ? 1024 : _is);
		reallocate_memory(true);
	}
	
	splay_tree(uint32_t _is, spt_reallocator _a, spt_deallocator _d, void *_data=NULL, size_t _len=0, void *_context=NULL) : reallocator(_a), deallocator(_d), context(_context) {
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
			}
		} else {
			reallocate_memory(true);
		}
	}
	
	~splay_tree() {
		if (deallocator) {
			deallocator(context, t_info);
		} else {
			inter_free(n_buff);
		}
	}
	
	void insert(const T &key) {
		if (t_info->t_size == t_info->b_size - 1 && reallocate_memory(false) == false) {
			return; // malloc fail
		}
		
		++t_info->t_size;
		/* Insert i into the tree t, unless it's already there.	*/
		/* Return a pointer to the resulting tree.				 */
		node_ptr n = next_free_node();
		get_node(n) = node(key);

		if (t_info->root_ptr == 0) {
			t_info->root_ptr = n;
			return;
		}
		
		splay(key, t_info->root_ptr);
		if (get_node_key(t_info->root_ptr) > key) {
			get_node_lc(n) = get_node_lc(t_info->root_ptr);
			get_node_rc(n) = t_info->root_ptr;
			get_node_lc(t_info->root_ptr) = 0;
			t_info->root_ptr = n;
		} else if (get_node_key(t_info->root_ptr) < key) {
			get_node_rc(n) = get_node_rc(t_info->root_ptr);
			get_node_lc(n) = t_info->root_ptr;
			get_node_rc(t_info->root_ptr) = 0;
			t_info->root_ptr = n;
		}
		else { /* We get here if it's already in the tree */
			/* Don't add it again					  */
			get_node_key(t_info->root_ptr) = key;
			free_node(n);
			--t_info->t_size;
		}
	}
	
	void remove(const T &key) {
		/* Deletes i from the tree if it's there.			   */
		/* Return a pointer to the resulting tree.			  */
		node_ptr x;
		if (t_info->root_ptr == 0) return;
		if (splay(key, t_info->root_ptr)) {			   /* found it */
			if (get_node_lc(t_info->root_ptr) == 0) {
				x = get_node_rc(t_info->root_ptr);
			} else {
				x = get_node_lc(t_info->root_ptr);
				splay(key, x);
				get_node_rc(x) = get_node_rc(t_info->root_ptr);
			}
			free_node(t_info->root_ptr);
			--t_info->t_size;
			t_info->root_ptr = x;
		}
	}
	
	bool exist(const T &key) {
		return (last_ptr = inter_find(key)) != 0;
	}
	
	// should check exist() first
	T &find() {
		return get_node_key(last_ptr);
	}
	
	uint32_t size() {
		return t_info->t_size;
	}
	
	void enumerate(void (^callback)(const T& key)) {
		inter_enumerate(t_info->root_ptr, callback);
	}
};

#endif /* splay_tree_h */
