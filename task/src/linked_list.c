#include "linked_list.h"
#include <stdlib.h> // 【新增】补齐 malloc, free
#include <string.h> // 【新增】补齐 strcmp
#include "common.h" // ← 确保包含状态码（如 STATUS_SUCCESS）等公共定义
/**
 * @brief 初始化双向链表
 * @param l 指向链表结构体的指针
 */
void List_Init(LinkedList *l)
{
    l->head = l->tail = NULL; // 头尾指针初始化为空
    l->size = 0;              // 链表节点数量初始化为 0
}
/**
 * @brief 在链表尾部插入一条新的课程记录（尾插法）
 * @param l 指向链表结构体的指针
 * @param rec 指向要插入的课程记录数据的指针
 * @return 成功返回 STATUS_SUCCESS，内存分配失败返回 STATUS_ERR_MEMORY
 */
int List_Insert(LinkedList *l, const CourseRecord *rec)
{
    // 1. 为新节点分配内存
    LinkedListNode *n = (LinkedListNode *)malloc(sizeof(LinkedListNode));
    if (!n)
        return STATUS_ERR_MEMORY; // 内存分配失败，返回错误码
                                  // 2. 初始化新节点的数据和指针
    n->data = *rec;               // 拷贝传入的记录数据
    n->next = NULL;               // 新节点将作为尾节点，next 指向 NULL
                                  // 3. 将新节点链接到链表中
    if (l->size == 0)             // 如果链表为空（第一个节点）
    {
        n->prev = NULL;
        l->head = l->tail = n; // 头尾指针都指向这个新节点
    }
    else // 如果链表不为空
    {
        n->prev = l->tail; // 新节点的前驱指向当前的尾节点
        l->tail->next = n; // 当前尾节点的后继指向新节点
        l->tail = n;       // 更新链表的尾指针，使其指向新节点
    }
    // 4. 更新链表长度
    l->size++;
    return STATUS_SUCCESS;
}
/**
 * @brief 根据学号和课程号搜索链表中的节点
 * @param l 指向链表结构体的指针
 * @param s 目标学号字符串
 * @param c 目标课程号字符串
 * @return 找到则返回指向该节点的指针，未找到返回 NULL
 */
LinkedListNode *List_SearchByKey(LinkedList *l, const char *s, const char *c)
{
    LinkedListNode *curr = l->head; // 从头节点开始遍历
    while (curr)
    {
        // 如果学号和课程号都匹配，则找到目标节点
        if (!strcmp(curr->data.student_id, s) && !strcmp(curr->data.course_id, c))
            return curr;
        curr = curr->next; // 移动到下一个节点继续查找
    }
    return NULL; // 遍历结束未找到，返回 NULL
}
/**
 * @brief 根据学号和课程号删除链表中的指定节点
 * @param list 指向链表结构体的指针
 * @param stu_id 目标学号字符串
 * @param cou_id 目标课程号字符串
 * @return 成功删除返回 STATUS_SUCCESS，未找到或参数无效返回 STATUS_ERR_NOT_FOUND
 */
int List_DeleteByKey(LinkedList *list, const char *stu_id, const char *cou_id)
{
    // 参数合法性检查，防止空指针解引用
    if (!list || !stu_id || !cou_id)
        return STATUS_ERR_NOT_FOUND;

    LinkedListNode *current = list->head; // 从头节点开始查找

    while (current)
    {
        // 查找学号和课程号均匹配的节点
        if (strcmp(current->data.student_id, stu_id) == 0 &&
            strcmp(current->data.course_id, cou_id) == 0)
        {
            // 正确更新前驱和后继节点的指针，将当前节点从链表中摘除
            // 1. 处理前驱节点的 next 指针
            if (current->prev)
                current->prev->next = current->next; // 有前驱，让前驱指向后继
            else
                list->head = current->next; // 无前驱（即是头节点），更新头指针
            // 2. 处理后继节点的 prev 指针
            if (current->next)
                current->next->prev = current->prev; // 有后继，让后继指向前驱
            else
                list->tail = current->prev; // 无后继（即是尾节点），更新尾指针
            // 3. 释放当前节点内存并更新链表长度
            free(current);
            list->size--;
            return STATUS_SUCCESS; // 删除成功，返回
        }
        current = current->next; // 未匹配，继续查找下一个节点
    }

    return STATUS_ERR_NOT_FOUND; // 遍历结束未找到匹配节点
}
/**
 * @brief 销毁整个链表，释放所有节点占用的内存
 * @param l 指向链表结构体的指针
 */
void List_Destroy(LinkedList *l)
{
    LinkedListNode *c = l->head; // 从当前头节点开始
    while (c)
    {
        LinkedListNode *n = c->next; // 先保存下一个节点的地址，防止当前节点释放后丢失后续链表
        free(c);                     // 释放当前节点内存
        c = n;                       // 移动到下一个节点继续释放
    }
    // 重置链表状态，防止野指针
    l->head = l->tail = NULL;
    l->size = 0;
}
/**
 * @brief 遍历链表，对每个节点的数据执行指定的回调操作。时间复杂度 O(N)
 *
 * @param list  指向链表结构体的指针
 * @param visit 回调函数指针，接收指向课程记录的常量指针
 * @note 通过函数指针实现业务逻辑与底层数据结构的解耦，适用于全量统计与导出
 */
void List_Traverse(LinkedList *list, void (*visit)(const CourseRecord *rec))
{
    if (!list || !visit)
        return; // 防御性编程：空指针检查

    LinkedListNode *curr = list->head;
    while (curr)
    {
        visit(&curr->data); // 将当前节点的数据域地址传给回调函数
        curr = curr->next;  // 移动到下一个节点
    }
}