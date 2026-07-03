#include "hash_table.h"
#include <stdio.h>  // 【新增】补齐 printf, exit
#include <stdlib.h> // 【新增】补齐 malloc, calloc, free
#include <string.h> // 【新增】补齐 strcmp
#include "common.h" // ← 确保包含
/*
 * 哈希函数 (基于经典的 DJB2 算法)
 * 将学号 (s) 和课程号 (c) 的字符依次混合计算哈希值
 * 返回值：哈希表中的数组索引
 */
static unsigned int HashFunc(const char *s, const char *c)
{
    unsigned long long h = 5381; // DJB2 算法的初始素数种子
                                 // 1. 遍历学号字符串，计算哈希值
    while (*s)
    {
        h = ((h << 5) + h) + (*s); // 等价于 h = h * 33 + *s，左移5位加自身效率更高
        s++;
    }
    // 2. 继续遍历课程号字符串，将两部分信息融合
    while (*c)
    {
        h = ((h << 5) + h) + (*c);
        c++;
    }
    // 3. 对哈希表大小取模，确保索引在 [0, HASH_TABLE_SIZE-1] 范围内
    return (unsigned int)(h % HASH_TABLE_SIZE);
}
/*
 * 初始化哈希表
 * 为哈希表的桶（数组）分配内存，并初始化当前记录数为 0
 */
void Hash_Init(HashTable *ht)
{
    // 使用 calloc 分配内存，它会自动将分配的内存清零（即所有指针初始为 NULL）
    ht->buckets = (HashNode **)calloc(HASH_TABLE_SIZE, sizeof(HashNode *));
    if (!ht->buckets)
    {
        printf("哈希表内存分配失败\n"); // 【修复】恢复 \n
        exit(STATUS_ERR_MEMORY);        // 内存分配严重错误，直接终止程序
    }
    ht->size = 0; // 初始元素个数为 0
}
/*
 * 向哈希表中插入一条课程记录
 * 返回值：STATUS_SUCCESS (成功) / STATUS_ERR_DUPLICATE (重复) / STATUS_ERR_MEMORY (内存不足)
 */
int Hash_Insert(HashTable *ht, const CourseRecord *rec)
{
    // 防重校验：先查找是否已存在相同的学号和课程号记录
    if (Hash_Search(ht, rec->student_id, rec->course_id))
        return STATUS_ERR_DUPLICATE;
    // 计算该记录应存放的哈希索引
    unsigned int idx = HashFunc(rec->student_id, rec->course_id);
    // 为新节点分配内存
    HashNode *n = (HashNode *)malloc(sizeof(HashNode));
    if (!n)
        return STATUS_ERR_MEMORY;
    // 填充节点数据
    n->data = *rec;
    // 【头插法】将新节点插入到对应桶的链表头部
    n->next = ht->buckets[idx];
    ht->buckets[idx] = n;
    ht->size++; // 哈希表元素总数加 1
    return STATUS_SUCCESS;
}
/*
 * 在哈希表中查找指定的课程记录
 * 根据学号 (s) 和课程号 (c) 进行精确匹配
 * 返回值：找到的节点指针，若未找到则返回 NULL
 */
HashNode *Hash_Search(HashTable *ht, const char *s, const char *c)
{
    // 1. 定位到对应的桶（链表头节点）
    HashNode *curr = ht->buckets[HashFunc(s, c)];
    // 2. 遍历该桶下的链表进行查找
    while (curr)
    {
        // 如果学号和课程号都完全匹配，则找到目标节点
        if (!strcmp(curr->data.student_id, s) && !strcmp(curr->data.course_id, c))
            return curr;
        curr = curr->next; // 继续检查下一个节点（处理哈希冲突）
    }
    return NULL; // 遍历完链表未找到
}
/*
 * 从哈希表中删除指定的课程记录
 * 根据学号 (s) 和课程号 (c) 查找并安全释放节点内存
 */
int Hash_Delete(HashTable *ht, const char *s, const char *c)
{
    unsigned int idx = HashFunc(s, c);               // 计算哈希索引
    HashNode *curr = ht->buckets[idx], *prev = NULL; // curr 为当前节点，prev 为前驱节点
                                                     // 遍历链表寻找目标节点
    while (curr)
    {
        // 找到匹配的节点
        if (!strcmp(curr->data.student_id, s) && !strcmp(curr->data.course_id, c))
        {
            // 从链表中摘除当前节点
            if (prev)
                prev->next = curr->next; // 如果不是头节点：前驱节点的 next 跨过当前节点
            else
                ht->buckets[idx] = curr->next; // 如果是头节点：更新桶的头指针
            free(curr);                        // 释放被删除节点的内存
            ht->size--;                        // 哈希表元素总数减 1
            return STATUS_SUCCESS;
        }
        prev = curr;       // 记录前驱节点
        curr = curr->next; // 移动到下一个节点
    }
    return STATUS_ERR_NOT_FOUND; // 未找到要删除的记录
}
/*
 * 销毁哈希表，释放所有占用的内存，防止内存泄漏
 */
void Hash_Destroy(HashTable *ht)
{
    if (!ht->buckets)
        return; // 如果桶数组为空，直接返回
                // 遍历每一个桶
    for (int i = 0; i < HASH_TABLE_SIZE; i++)
    {
        HashNode *c = ht->buckets[i];
        // 遍历并释放该桶对应的链表中的所有节点
        while (c)
        {
            HashNode *n = c->next; // 暂存下一个节点的地址，防止断链
            free(c);               // 释放当前节点
            c = n;                 // 移动到下一个节点
        }
    }
    free(ht->buckets);  // 释放桶数组（指针数组）本身的内存
    ht->buckets = NULL; // 置空指针，防止产生野指针
    ht->size = 0;       // 重置元素个数为 0
}
/**
 * @brief 遍历哈希表中的所有有效记录，执行指定的回调操作。时间复杂度 O(N+M)
 *
 * @param ht    指向哈希表结构体的指针
 * @param visit 回调函数指针
 * @note 需遍历所有桶(M)及桶内链表(N)，适用于海量数据下的全表扫描与多维统计分析
 */
void Hash_Traverse(HashTable *ht, void (*visit)(const CourseRecord *rec))
{
    if (!ht || !ht->buckets || !visit)
        return; // 防御性编程

    // 遍历每一个桶
    for (int i = 0; i < HASH_TABLE_SIZE; i++)
    {
        HashNode *curr = ht->buckets[i];
        // 遍历该桶挂载的冲突链表
        while (curr)
        {
            visit(&curr->data);
            curr = curr->next;
        }
    }
}