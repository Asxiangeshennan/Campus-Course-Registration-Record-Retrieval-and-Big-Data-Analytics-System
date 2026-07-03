/**
 * @file linked_list.h
 * @brief 双向链表模块头文件，用于存储和管理课程记录数据
 */
#ifndef LINKED_LIST_H
#define LINKED_LIST_H
/* 引入公共头文件，其中应包含 CourseRecord 等基础数据类型的定义 */
#include "common.h"
/**
 * @brief 双向链表节点结构体
 */
typedef struct LinkedListNode
{
    CourseRecord data;           /* 节点存储的有效数据：课程记录 */
    struct LinkedListNode *prev; /* 前驱指针：指向前一个节点 */
    struct LinkedListNode *next; /* 后继指针：指向后一个节点 */
} LinkedListNode;
/**
 * @brief 双向链表管理结构体
 */
typedef struct
{
    LinkedListNode *head; /* 头指针：指向链表的第一个节点 */
    LinkedListNode *tail; /* 尾指针：指向链表的最后一个节点 */
    int size;             /* 链表长度：记录当前链表中的节点总数 */
} LinkedList;
/**
 * @brief 初始化链表
 * @param list 指向待初始化的链表结构体的指针
 * @note 调用此函数后，链表的头尾指针将被置空，大小置为0
 */
void List_Init(LinkedList *list);
/**
 * @brief 向链表中插入一条新的课程记录
 * @param list 指向目标链表的指针
 * @param rec 指向待插入的课程记录数据的指针
 * @return 操作结果状态码（通常成功返回1/非0，失败返回0，具体依实现而定）
 */
int List_Insert(LinkedList *list, const CourseRecord *rec);
/**
 * @brief 根据学号和课程号删除链表中的指定节点
 * @param list 指向目标链表的指针
 * @param stu_id 目标学生的学号 (字符串)
 * @param cou_id 目标课程的编号 (字符串)
 * @return 删除成功返回1/非0，未找到匹配项或删除失败返回0
 */
int List_DeleteByKey(LinkedList *list, const char *stu_id, const char *cou_id);
/**
 * @brief 根据学号和课程号在链表中查找指定的节点
 * @param list 指向目标链表的指针
 * @param stu_id 目标学生的学号 (字符串)
 * @param cou_id 目标课程的编号 (字符串)
 * @return 查找成功返回指向该节点的指针，未找到则返回 NULL
 */
LinkedListNode *List_SearchByKey(LinkedList *list, const char *stu_id, const char *cou_id);
/**
 * @brief 销毁链表，遍历并释放所有节点占用的动态内存
 * @param list 指向待销毁的链表结构体的指针
 * @note 销毁后建议将链表指针置空或重新调用 List_Init 初始化
 */
void List_Destroy(LinkedList *list);
/**
 * @brief 遍历链表，对每个节点的数据执行指定的回调操作
 * @param list  指向目标链表的指针
 * @param visit 回调函数指针
 */
void List_Traverse(LinkedList *list, void (*visit)(const CourseRecord *rec));
#endif