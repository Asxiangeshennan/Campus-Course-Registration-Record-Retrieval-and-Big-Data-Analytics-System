/**
 * @file hash_table.h
 * @brief 哈希表模块头文件
 *
 * 本文件定义了用于存储和管理课程记录(CourseRecord)的哈希表数据结构及相关操作函数。
 * 底层采用“链地址法（拉链法）”来解决哈希冲突。
 */
#ifndef HASH_TABLE_H
#define HASH_TABLE_H
#include "common.h"
/**
 * @brief 哈希表的容量（桶的数量）
 * @note 200003 是一个质数。在哈希表设计中，使用质数作为表长有助于使哈希值分布更加均匀，从而减少哈希冲突。
 */
#define HASH_TABLE_SIZE 200003
/**
 * @brief 哈希表节点结构体（单链表节点）
 */
typedef struct HashNode
{
    CourseRecord data;     // 数据域：存储具体的课程记录信息
    struct HashNode *next; // 指针域：指向下一个同义词节点，用于处理哈希冲突
} HashNode;
/**
 * @brief 哈希表主体管理结构体
 */
typedef struct
{
    HashNode **buckets; // 桶数组：指针数组，每个元素指向一个单链表的头节点
    int size;           // 记录哈希表中当前实际存储的元素总个数（用于统计或判断负载因子）
} HashTable;
/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化哈希表
 * @param ht 指向哈希表结构体的指针
 * @note 此函数会动态分配 buckets 数组的内存，并将所有桶的指针初始化为 NULL，size 置为 0。
 */
void Hash_Init(HashTable *ht);
/**
 * @brief 向哈希表中插入一条课程记录
 * @param ht  指向哈希表结构体的指针
 * @param rec 指向要插入的课程记录的常量指针
 * @return 操作结果状态码（例如：成功返回 1，失败或已存在返回 0，具体视 .c 文件实现而定）
 */
int Hash_Insert(HashTable *ht, const CourseRecord *rec);
/**
 * @brief 从哈希表中删除指定的课程记录
 * @param ht     指向哈希表结构体的指针
 * @param stu_id 学生学号（与 cou_id 共同构成查找/删除的复合主键）
 * @param cou_id 课程编号
 * @return 操作结果状态码（例如：删除成功返回 1，未找到记录返回 0）
 */
int Hash_Delete(HashTable *ht, const char *stu_id, const char *cou_id);
/**
 * @brief 在哈希表中查找指定的课程记录
 * @param ht     指向哈希表结构体的指针
 * @param stu_id 学生学号
 * @param cou_id 课程编号
 * @return 若找到，返回指向对应 HashNode 的指针；若未找到，返回 NULL
 */
HashNode *Hash_Search(HashTable *ht, const char *stu_id, const char *cou_id);
/**
 * @brief 销毁哈希表，释放所有动态分配的内存，防止内存泄漏
 * @param ht 指向哈希表结构体的指针
 * @note 此函数会遍历所有桶，释放链表中的每一个节点，最后释放 buckets 数组本身。
 */
void Hash_Destroy(HashTable *ht);
/**
 * @brief 遍历哈希表中的所有有效记录，执行指定的回调操作。时间复杂度 O(N+M)
 *
 * @param ht    指向哈希表结构体的指针
 * @param visit 回调函数指针
 * @note 需遍历所有桶(M)及桶内链表(N)，适用于海量数据下的全表扫描与多维统计分析
 */
void Hash_Traverse(HashTable *ht, void (*visit)(const CourseRecord *rec));
#endif