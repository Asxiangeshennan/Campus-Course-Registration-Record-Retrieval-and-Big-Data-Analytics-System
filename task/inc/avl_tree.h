#ifndef AVL_TREE_H
#define AVL_TREE_H

#include "model.h"  // 引入数据模型头文件，提供 CourseRecord 结构体的定义
#include "status.h" // 引入状态码头文件，提供 STATUS_SUCCESS、STATUS_FAILURE 等状态常量
/*
 * AVL 树节点结构体
 * 用于存储课程记录（CourseRecord），并维护二叉搜索树及平衡性质
 */
typedef struct AVLTreeNode
{
    CourseRecord data;         // 节点存储的数据：一条课程记录
    struct AVLTreeNode *left;  // 指向左子节点的指针
    struct AVLTreeNode *right; // 指向右子节点的指针
    int height;                // 当前节点的高度（叶子节点高度为 1，空节点高度为 0）
} AVLTreeNode;

/*
 * AVL 树插入操作
 * 将一条课程记录插入到 AVL 树中，保持二叉搜索树性质和平衡性
 *
 * @param node    当前子树的根节点指针
 * @param rec     指向待插入课程记录的指针（只读）
 * @param success 输出参数，插入成功时置为 1，失败（如重复插入）时置为 0
 * @return        插入后新的子树根节点指针（可能因旋转而改变）
 */
AVLTreeNode *AVL_Insert(AVLTreeNode *node, const CourseRecord *rec, int *success);
/*
 * AVL 树删除操作
 * 根据学号（stu_id）和课程号（cou_id）查找并删除对应的课程记录节点
 *
 * @param node    当前子树的根节点指针
 * @param stu_id  待删除记录的学号（字符串）
 * @param cou_id  待删除记录的课程号（字符串）
 * @param success 输出参数，删除成功时置为 1，未找到目标时置为 0
 * @return        删除后新的子树根节点指针（可能因旋转而改变）
 */
AVLTreeNode *AVL_Delete(AVLTreeNode *node, const char *stu_id, const char *cou_id, int *success);
/*
 * AVL 树查找操作
 * 根据学号和课程号在 AVL 树中搜索对应的节点
 *
 * @param node   当前子树的根节点指针
 * @param stu_id 目标学号（字符串）
 * @param cou_id 目标课程号（字符串）
 * @return       找到则返回指向该节点的指针，未找到则返回 NULL
 */
AVLTreeNode *AVL_Search(AVLTreeNode *node, const char *stu_id, const char *cou_id);
/*
 * AVL 树销毁操作
 * 递归释放整棵 AVL 树中所有节点占用的内存
 *
 * @param node 当前子树的根节点指针
 */
void AVL_Destroy(AVLTreeNode *node);
/**
 * @brief 中序遍历 AVL 树，对每个节点的数据执行指定的回调操作。时间复杂度 O(N)
 *
 * @param node  当前子树的根节点
 * @param visit 回调函数指针
 * @note 中序遍历可保证数据按联合主键（学号+课程号）的字典序严格有序输出，天然支持范围查询与有序导出
 */
void AVL_InOrderTraverse(AVLTreeNode *node, void (*visit)(const CourseRecord *rec));
#endif // AVL_TREE_H