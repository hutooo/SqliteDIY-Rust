#include <stdlib.h>  // EXIT_FAILURE
#include <stdio.h>
#include <string.h>
#include "btree.h"

// * =====
// * Common Node Header Layout
// * =====

// 标识节点类型
const uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);

// 节点类型的偏移量
const uint32_t NODE_TYPE_OFFSET = 0;

// 标识是否为根节点
const uint32_t IS_ROOT_SIZE = sizeof(uint8_t);

// 标识是否为根节点的偏移量
const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;

// 父节点指针
const uint32_t PARENT_POINT_SIZE = sizeof(uint32_t);

// 父节点指针偏移量
const uint32_t PARENT_POINT_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;

// 通用节点头部大小
const uint32_t COMMON_NODE_HEADER_SIZE = NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINT_SIZE;

// * =====
// * Leaf Node Format
// * 除了 通用头节点信息, 叶节点还需要存储自身包含的 cell [ k/v pairs ] 数量
// * =====

// 标识叶节点cell数量
const uint32_t LEAF_NODE_NUMS_CELLS_SIZE = sizeof(uint32_t);

// 叶节点cell数量的偏移量
const uint32_t LEAF_NODE_NUMS_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;

// 叶节点末尾指向下一个叶节点的指针
const uint32_t LEAF_NODE_NEXT_LEAF_SIZE = sizeof(uint32_t);

// 叶节点末尾指向下一个叶节点的指针的偏移量
const uint32_t LEAF_NODE_NEXT_LEAF_OFFSET = LEAF_NODE_NUMS_CELLS_OFFSET + LEAF_NODE_NUMS_CELLS_SIZE;

// 叶节点头部大小
const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUMS_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE;


// * =====
// * Leaf Node Body Layout
// * 叶节点体布局, 是cell数组
// * =====

// 叶节点的键
const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);

// 叶节点键的偏移量
const uint32_t LEAF_NODE_KEY_OFFSET = 0;

// 叶节点的值[行记录]
const uint32_t LEAF_NODE_VALUE_SIZE = ROW_SIZE;

// 叶节点的值偏移量
const uint32_t LEAF_NODE_VALUE_OFFSET = LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE;

// 叶节点cell的大小
const uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;

// 单页 叶节点cells的最大空间
// 当剩余空间无法放下完整cell的时候，我们放弃剩下的空间，避免将cell分裂到不同的节点
const uint32_t LEAF_NODE_SPACE_FOR_CELLS = PAGE_SIZE - LEAF_NODE_HEADER_SIZE;

// 叶节点最大cells数量
const uint32_t LEAF_NODE_MAX_CELLS = LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;

// n个cells的节点，n+1时分裂，如果时奇数, 就把新cell分配到左节点[随意]
const uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2;
const uint32_t LEAF_NODE_LEFT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT;

// * =====
// * Internal Node Layout
// * 枝节点/内节点/内部节点 布局
// * =====

/// |# internal node layers |max # leaf nodes | Size of all leaf nodes|
/// |---|---|---|
/// |    0                  | 511^0 = 1 	   | ~4 KB |
/// |    1                  | 511^1 = 512 	   | ~2 MB |
/// |    2                  | 511^2 = 261,121 	| ~1 GB |
/// |    3                  | 511^3 = 133,432,831  | ~550 GB |

// 内节点 键的数量
const uint32_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);

// 内节点 键的数量 偏移量
const uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET = COMMON_NODE_HEADER_SIZE;

// 内节点 右子节点指针
const uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t);

// 内节点 右子节点 偏移量
const uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET = INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE;

// 内节点头部大小
const uint32_t INTERNAL_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE;

// 内节点 键
const uint32_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t);

// 内节点 子节点指针
const uint32_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t);

// 内节点 Cell
const uint32_t INTERNAL_NODE_CELL_SIZE = INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE;


// * =====
// * 访问叶节点字段/域
// * =====

uint32_t* leaf_node_next_leaf(void* node) {
  return node + LEAF_NODE_NEXT_LEAF_OFFSET;
}

// 返回叶节点的cell数量
uint32_t* leaf_node_num_cells(void* node) {
    return (uint32_t*)(node + LEAF_NODE_NUMS_CELLS_OFFSET);
}

// 返回叶节点的cells
void* leaf_node_cell(void* node, uint32_t cell_num) {
    return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

// 叶节点的键[cell以key开头]
uint32_t* leaf_node_key(void* node, uint32_t cell_num) {
    return (uint32_t*)leaf_node_cell(node, cell_num);
}

// 叶节点的cell值[键+偏移]
void* leaf_node_value(void* node, uint32_t cell_num) {
    return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

// 初始化叶节点[将cell数量置为0]
void initialize_leaf_node(void* node) {
    // 初始化节点类型
    set_node_type(node, NODE_LEAF);
    // 设置是否为根节点
    set_node_root(node, false);
    // 初始cells数量为0
    *leaf_node_num_cells(node) = 0;
    // 初始化右侧叶节点的指针
    *leaf_node_next_leaf(node) = 0;
}

// 到回收空闲页之前，新的页总是生成在文件末尾
uint32_t get_unused_page_num(Pager* pager) {
    return pager->num_pages;
}

// 获取内节点的keys数量
uint32_t* internal_node_num_keys(void* node) {
    return (uint32_t*)(node + INTERNAL_NODE_NUM_KEYS_OFFSET);
}

// 获取内节点的右子节点指针
uint32_t* internal_node_right_child(void* node) {
    return (uint32_t*)(node + INTERNAL_NODE_RIGHT_CHILD_OFFSET);
}

// 获取内节点的指定num的cell
uint32_t* internal_node_cell(void* node, uint32_t cell_num) {
    return (uint32_t*)(node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE);
}

// 获取内节点中指定的子节点指针
uint32_t* internal_node_child(void* node, uint32_t child_num) {
    // key的总数
    uint32_t keys_count = *internal_node_num_keys(node);
    
    if (child_num > keys_count) {
        printf("Tried to access child_num %d > num_keys %d\n", child_num, keys_count);
        exit(EXIT_FAILURE);
    } else if (child_num == keys_count) {
        return internal_node_right_child(node);
    } else {
        return internal_node_cell(node, child_num);
    }
}

// 获取内节点中指定的key
uint32_t* internal_node_key(void* node, uint32_t key_num) {
    return (void*)internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
}

// 获取节点内的最大key
// 对于内节点，是最右侧的key. 对于叶节点，则在最大索引处
uint32_t get_node_max_key(void* node) {
    switch (get_node_type(node))
    {
    case NODE_INTERNAL:
        return *internal_node_right_child(node);
    case NODE_LEAF:
        return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
    }
}

// 初始化内节点
void initialize_internal_node(void* node) {
    // 设置节点类型
    set_node_type(node, NODE_INTERNAL);
    // 设置是否为根节点
    set_node_root(node, false);
    // 设置key数量
    *internal_node_num_keys(node) = 0;
}


// 创建新的根节点
// 旧的根节点赋值到新的页，成为左子节点
// 传入右子节点的地址，重新初始化root页，包含新的根节点
// 新的跟节点，指向两个子节点
void create_new_root(Table* table, uint32_t right_child_page_num) {
    // 根节点
    void* root = get_page(table->pager, table->root_page_no);
    // 右子节点
    // void* right_child = get_page(table->pager, right_child_page_num);
    // 生成新的左子节点页码
    uint32_t left_child_page_num = get_unused_page_num(table->pager);
    // 为新的左子节点分配页空间
    void* left_child = get_page(table->pager, left_child_page_num);

    // 从旧的根节点拷贝数据到新的左子节点
    memcpy(left_child, root, PAGE_SIZE);
    // 不再将左子节点作为根节点
    set_node_root(left_child, false);

    // 最后 初始化根节点 作为一个新的内部节点，指向两个子节点
    initialize_internal_node(root);
    set_node_root(root, true);

    *internal_node_num_keys(root) = 1;
    *internal_node_child(root, 0) = left_child_page_num;

    uint32_t left_child_max_key = get_node_max_key(left_child);

    *internal_node_key(root, 0) = left_child_max_key;
    *internal_node_right_child(root) = right_child_page_num;
}

// 是否是根节点
bool is_node_root(void* node) {
    uint8_t value = *((uint8_t*)(node + IS_ROOT_OFFSET));
    return (bool)value;
}

// 设置是否为根节点
void set_node_root(void* node, bool is_root){
    uint8_t value = (uint8_t)is_root;
    *(uint8_t*)(node + IS_ROOT_OFFSET) = value;
}

// 创建一个新的节点,并移动一半的cells, 在分裂的两半中的一方插入新值，更新父节点或者创建一个新的父节点
void leaf_node_split_and_insert(Cursor* cursor, uint32_t key, Row* value) {
    // 缓存旧节点，创建并初始化新节点
    void* old_node = get_page(cursor->table->pager, cursor->page_no);
    // 获取未使用的page号
    uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
    // 创建新节点
    void* new_node = get_page(cursor->table->pager, new_page_num);
    // 初始化新的叶节点
    initialize_leaf_node(new_node);
    *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
    *leaf_node_next_leaf(old_node) = new_page_num;

    // 将所有的cells复制到新的地方
    // 所有已经存在的keys需要被均匀分到 新[right] 旧[left] 两个节点
    // 对于右侧节点的每一个key，需要移动到正确的位置
    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--) {
        // 新节点
        void* dst_node;
        if (i >= LEAF_NODE_LEFT_SPLIT_COUNT) {
            dst_node = new_node;
        } else {
            dst_node = old_node;
        }

        // 计算cell的编号
        uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
        // 返回cell
        void* dst_cell = leaf_node_cell(dst_node, index_within_node);

        // 如果cell匹配, 则进行序列化[value值插入cell]
        if (i == cursor->cell_no) {
            serialize_row(value, leaf_node_value(dst_node, index_within_node));
            *leaf_node_key(dst_node, index_within_node) = key;
        } else if (i > cursor->cell_no) {
            memcpy(dst_cell, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
        } else {
            memcpy(dst_cell, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
        }
    }

    // 更新node头部中的cell数量
    *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
    *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;

    // 更新父节点 [如果原始节点是根节点，则没有父节点，需要创建父节点]
    if (is_node_root(old_node)) {
        // 此时已经分配了一半的cells到右子节点
        // 将右子节点作为输入，分配新的页，存储左子节点
        return create_new_root(cursor->table, new_page_num);
    } else {
        printf("Need to implement updating parent after split\n");
        exit(EXIT_FAILURE);
    }
}


// 插入cell到节点
void leaf_node_insert(Cursor* cursor, uint32_t key, Row* value) {
    // 查找到游标指向 所在的节点
    void* node = get_page(cursor->table->pager, cursor->page_no);

    // 获取当前节点的cell数量
    uint32_t num_cells = *leaf_node_num_cells(node);

    // -- 目前还没有进行节点的拆分, 所以可能会超过cell最大数量
    // split leaf node
    if (num_cells >= LEAF_NODE_MAX_CELLS) {
        // cell数量超过 叶节点的最大单元数
        // printf("Need to implement splitting a leaf node.\n");
        // exit(EXIT_FAILURE);

        // 进行分裂
        leaf_node_split_and_insert(cursor, key, value);
        return;
    }

    if (cursor->cell_no < num_cells) {
        // 将大于光标指向的cell号的记录 向后平移
        // 为新的cell腾出空间
        for (uint32_t i = num_cells; i > cursor->cell_no; i--) {
            memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1), LEAF_NODE_CELL_SIZE);
        }
    }
    // cell数量+1
    *(leaf_node_num_cells(node)) += 1;

    // cell键 赋值
    *(leaf_node_key(node, cursor->cell_no)) = key;

    // 数据序列化到 cell值中
    serialize_row(value, leaf_node_value(node, cursor->cell_no));
}

// 叶节点查询
Cursor* leaf_node_find(Table* table, uint32_t page_num, uint32_t key) {
    void* node = get_page(table->pager, page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_no = page_num;

    // 二分查询
    uint32_t min_index = 0;
    uint32_t one_past_max_index = num_cells;

    while(one_past_max_index != min_index) {
        uint32_t index = (min_index + one_past_max_index) / 2;
        uint32_t key_at_index = *leaf_node_key(node, index);
        if (key == key_at_index) {
            cursor->cell_no = index;
            return cursor;
        }
        if (key < key_at_index) {
            one_past_max_index = index;
        } else {
            min_index = index + 1;
        }
    }

    cursor->cell_no = min_index;
    return cursor;
}

// 查找内节点
Cursor* internal_node_find(Table* table, uint32_t page_num, uint32_t key) {
    void* node = get_page(table->pager, page_num);
    uint32_t keys_count = *internal_node_num_keys(node);

    // 二分查找
    uint32_t min_idx = 0;
    uint32_t max_idx = keys_count; // keys_count 个 child, 再加一个最右child

    while (min_idx != max_idx) {
        uint32_t idx = (min_idx + max_idx) / 2;
        uint32_t key_right = *internal_node_key(node, idx);
        if (key_right >= key) {
            max_idx = idx;
        } else {
            min_idx = idx + 1;
        }
    }

    // 根据内节点的子节点的不同类型不同处理
    uint32_t child_no = *internal_node_child(node, min_idx);
    void* child_node = get_page(table->pager, child_no);
    switch (get_node_type(child_node))
    {
    case NODE_LEAF:
        return leaf_node_find(table, child_no, key);
    case NODE_INTERNAL:
        return internal_node_find(table, child_no, key);
    }
}

// 获取节点类型
NodeType get_node_type(void* node) {
    uint8_t value = *((uint8_t*)(node + NODE_TYPE_OFFSET));
    return (NodeType)value;
}

// 更改节点类型
void set_node_type(void* node, NodeType type) {
    uint8_t value = (uint8_t)type;
    *((uint8_t*)(node + NODE_TYPE_OFFSET)) = value;
}
