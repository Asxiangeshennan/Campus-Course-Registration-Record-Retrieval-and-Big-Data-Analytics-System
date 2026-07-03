#include "avl_tree.h"
#include "common.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief 辅助函数：获取节点的高度
 *
 * @param node AVL树节点指针
 * @return int 节点的高度，若节点为空则返回 0
 */
static int GetHeight(AVLTreeNode *node)
{
    return node ? node->height : 0;
}

/**
 * @brief 辅助函数：获取两个整数中的最大值
 *
 * @param a 整数 a
 * @param b 整数 b
 * @return int 较大的值
 */
static int Max(int a, int b)
{
    return (a > b) ? a : b;
}

/**
 * @brief 辅助函数：右旋转 (Right Rotation)
 *        用于处理 LL 型不平衡 (左孩子的左子树过高)
 *
 * @param y 不平衡的节点 (旋转前的根节点)
 * @return AVLTreeNode* 旋转后的新根节点
 */
static AVLTreeNode *RotateRight(AVLTreeNode *y)
{
    AVLTreeNode *x = y->left;   // x 是 y 的左孩子
    AVLTreeNode *T2 = x->right; // T2 是 x 的右子树，旋转后需要挂载到 y 的左边
    // 执行旋转
    x->right = y; // y 变成 x 的右孩子
    y->left = T2; // T2 变成 y 的左子树
    // 更新高度：先更新 y (现在的子节点)，再更新 x (现在的根节点)
    y->height = Max(GetHeight(y->left), GetHeight(y->right)) + 1;
    x->height = Max(GetHeight(x->left), GetHeight(x->right)) + 1;

    return x; // 返回新的根节点
}

/**
 * @brief 辅助函数：左旋转 (Left Rotation)
 *        用于处理 RR 型不平衡 (右孩子的右子树过高)
 *
 * @param x 不平衡的节点 (旋转前的根节点)
 * @return AVLTreeNode* 旋转后的新根节点
 */
static AVLTreeNode *RotateLeft(AVLTreeNode *x)
{
    AVLTreeNode *y = x->right; // y 是 x 的右孩子
    AVLTreeNode *T2 = y->left; // T2 是 y 的左子树，旋转后需要挂载到 x 的右边
    // 执行旋转
    y->left = x;   // x 变成 y 的左孩子
    x->right = T2; // T2 变成 x 的右子树
    // 更新高度：先更新 x (现在的子节点)，再更新 y (现在的根节点)
    x->height = Max(GetHeight(x->left), GetHeight(x->right)) + 1;
    y->height = Max(GetHeight(y->left), GetHeight(y->right)) + 1;

    return y; // 返回新的根节点
}

/**
 * @brief 辅助函数：获取节点的平衡因子
 *        平衡因子 = 左子树高度 - 右子树高度
 *
 * @param node AVL树节点指针
 * @return int 平衡因子，若节点为空返回 0
 */
static int GetBalance(AVLTreeNode *node)
{
    return node ? GetHeight(node->left) - GetHeight(node->right) : 0;
}

/**
 * @brief 辅助函数：比较两个记录的键值 (学生ID 和 课程ID)
 *        先比较学生ID，若相同再比较课程ID，确保键值的唯一性和有序性
 *
 * @param stu_id1 第一个学生ID
 * @param cou_id1 第一个课程ID
 * @param stu_id2 第二个学生ID
 * @param cou_id2 第二个课程ID
 * @return int <0: 键值1 < 键值2; 0: 相等; >0: 键值1 > 键值2
 */
static int CompareKeys(const char *stu_id1, const char *cou_id1,
                       const char *stu_id2, const char *cou_id2)
{
    int cmp = strcmp(stu_id1, stu_id2);
    if (cmp != 0)
        return cmp;
    return strcmp(cou_id1, cou_id2);
}
/**
 * @brief 向 AVL 树中插入一条课程记录
 *
 * @param node 当前子树的根节点
 * @param rec 要插入的课程记录数据
 * @param success 用于返回操作状态的指针 (STATUS_SUCCESS, STATUS_ERR_DUPLICATE, STATUS_ERR_MEMORY)
 * @return AVLTreeNode* 插入并重新平衡后的子树根节点
 */
AVLTreeNode *AVL_Insert(AVLTreeNode *node, const CourseRecord *rec, int *success)
{ // 1. 找到空位置，创建新节点
    if (!node)
    {
        node = (AVLTreeNode *)malloc(sizeof(AVLTreeNode));
        if (!node)
        {
            if (success)
                *success = STATUS_ERR_MEMORY; // 内存分配失败
            return NULL;
        }
        node->data = *rec;
        node->left = NULL;
        node->right = NULL;
        node->height = 1; // 新节点高度为 1
        if (success)
            *success = STATUS_SUCCESS;
        return node;
    }
    // 2. 比较键值，决定向左还是向右递归插入
    int cmp = CompareKeys(rec->student_id, rec->course_id,
                          node->data.student_id, node->data.course_id);

    if (cmp < 0)
        node->left = AVL_Insert(node->left, rec, success);
    else if (cmp > 0)
        node->right = AVL_Insert(node->right, rec, success);
    else
    {
        // 键值已存在，不允许重复插入
        if (success)
            *success = STATUS_ERR_DUPLICATE;
        return node;
    }

    // 3. 插入后，向上回溯更新当前节点的高度
    node->height = 1 + Max(GetHeight(node->left), GetHeight(node->right));

    // 4. 获取平衡因子，检查是否失去平衡
    int balance = GetBalance(node);

    // 5. 如果失去平衡 (|balance| > 1)，进行相应的旋转操作

    // LL 型：左子树过高，且新节点插入在左孩子的左子树
    if (balance > 1 && CompareKeys(rec->student_id, rec->course_id,
                                   node->left->data.student_id, node->left->data.course_id) < 0)
        return RotateRight(node);

    // RR 型：右子树过高，且新节点插入在右孩子的右子树
    if (balance < -1 && CompareKeys(rec->student_id, rec->course_id,
                                    node->right->data.student_id, node->right->data.course_id) > 0)
        return RotateLeft(node);

    // LR 型：左子树过高，且新节点插入在左孩子的右子树 (先左旋左孩子，再右旋当前节点)
    if (balance > 1 && CompareKeys(rec->student_id, rec->course_id,
                                   node->left->data.student_id, node->left->data.course_id) > 0)
    {
        node->left = RotateLeft(node->left);
        return RotateRight(node);
    }

    // RL 型：右子树过高，且新节点插入在右孩子的左子树 (先右旋右孩子，再左旋当前节点)
    if (balance < -1 && CompareKeys(rec->student_id, rec->course_id,
                                    node->right->data.student_id, node->right->data.course_id) < 0)
    {
        node->right = RotateRight(node->right);
        return RotateLeft(node);
    }

    return node; // 树保持平衡，返回当前节点
}

/**
 * @brief 从 AVL 树中删除指定学生ID和课程ID的记录
 *
 * @param node 当前子树的根节点
 * @param stu_id 要删除的学生ID
 * @param cou_id 要删除的课程ID
 * @param success 用于返回操作状态的指针 (STATUS_SUCCESS, STATUS_ERR_NOT_FOUND)
 * @return AVLTreeNode* 删除并重新平衡后的子树根节点
 */
AVLTreeNode *AVL_Delete(AVLTreeNode *node, const char *stu_id, const char *cou_id, int *success)
{ // 1. 未找到要删除的节点
    if (!node)
    {
        if (success)
            *success = STATUS_ERR_NOT_FOUND;
        return NULL;
    }
    // 2. 递归查找要删除的节点
    int cmp = CompareKeys(stu_id, cou_id, node->data.student_id, node->data.course_id);
    if (cmp < 0)
        node->left = AVL_Delete(node->left, stu_id, cou_id, success);
    else if (cmp > 0)
        node->right = AVL_Delete(node->right, stu_id, cou_id, success);
    else
    {
        // 3. 找到要删除的节点，执行删除逻辑
        if (success)
            *success = STATUS_SUCCESS;
        // 标准指针替换法，杜绝值拷贝带来的内存风险
        if (!node->left || !node->right)
        {
            AVLTreeNode *temp = node->left ? node->left : node->right;
            if (!temp)
            {
                // 没有子节点：直接释放当前节点，返回 NULL 给父节点
                free(node);
                return NULL;
            }
            else
            {
                // 只有一个子节点：释放当前节点，用唯一的子节点替换当前节点
                free(node);
                return temp;
            }
        }
        else
        {
            // 有两个子节点：寻找右子树中的最小节点 (中序后继)
            AVLTreeNode *temp = node->right;
            while (temp && temp->left)
                temp = temp->left;
            // 将后继节点的数据拷贝到当前节点 (避免直接修改指针带来的复杂问题)
            node->data = temp->data;
            // 递归删除右子树中的那个后继节点
            // 注意：这里递归删除时 success 会被再次赋值，但因为删除后继必定成功，不影响最终结果
            node->right = AVL_Delete(node->right, temp->data.student_id, temp->data.course_id, success);
        }
    }
    // 如果节点在删除过程中被置空，直接返回
    if (!node)
        return NULL;
    // 4. 向上回溯，更新当前节点的高度
    node->height = 1 + Max(GetHeight(node->left), GetHeight(node->right));
    // 5. 获取平衡因子，检查是否需要旋转
    int balance = GetBalance(node);

    // LL 型平衡：左子树过高，且左孩子的平衡因子 >= 0
    if (balance > 1 && GetBalance(node->left) >= 0)
        return RotateRight(node);
    // LR 型平衡：左子树过高，且左孩子的平衡因子 < 0
    if (balance > 1 && GetBalance(node->left) < 0)
    {
        node->left = RotateLeft(node->left);
        return RotateRight(node);
    }

    // RR 型平衡：右子树过高，且右孩子的平衡因子 <= 0
    if (balance < -1 && GetBalance(node->right) <= 0)
        return RotateLeft(node);

    // RL 型平衡：右子树过高，且右孩子的平衡因子 > 0
    if (balance < -1 && GetBalance(node->right) > 0)
    {
        node->right = RotateRight(node->right);
        return RotateLeft(node);
    }

    return node;
}
/**
 * @brief 在 AVL 树中查找指定学生ID和课程ID的记录
 *
 * @param node 当前子树的根节点
 * @param stu_id 要查找的学生ID
 * @param cou_id 要查找的课程ID
 * @return AVLTreeNode* 找到的节点指针，若未找到则返回 NULL
 */
AVLTreeNode *AVL_Search(AVLTreeNode *node, const char *stu_id, const char *cou_id)
{
    if (!node)
        return NULL;

    int cmp = CompareKeys(stu_id, cou_id,
                          node->data.student_id, node->data.course_id);
    // 根据比较结果决定向左或向右查找
    if (cmp < 0)
        return AVL_Search(node->left, stu_id, cou_id);
    else if (cmp > 0)
        return AVL_Search(node->right, stu_id, cou_id);
    else
        return node; // 找到目标节点
}
/**
 * @brief 销毁整棵 AVL 树，释放所有节点内存
 *        采用后序遍历 (左 -> 右 -> 根) 确保子节点先于父节点被释放
 *
 * @param node 当前子树的根节点
 */
void AVL_Destroy(AVLTreeNode *node)
{
    if (!node)
        return;
    AVL_Destroy(node->left);  // 递归销毁左子树
    AVL_Destroy(node->right); // 递归销毁右子树
    free(node);               // 释放当前节点
}
/**
 * @brief 中序遍历 AVL 树，对每个节点的数据执行指定的回调操作。时间复杂度 O(N)
 *
 * @param node  当前子树的根节点
 * @param visit 回调函数指针
 * @note 中序遍历可保证数据按联合主键（学号+课程号）的字典序严格有序输出，天然支持范围查询与有序导出
 */
void AVL_InOrderTraverse(AVLTreeNode *node, void (*visit)(const CourseRecord *rec))
{
    if (!node || !visit)
        return; // 递归终止条件及空指针防御

    AVL_InOrderTraverse(node->left, visit);  // 1. 递归遍历左子树
    visit(&node->data);                      // 2. 访问当前节点数据
    AVL_InOrderTraverse(node->right, visit); // 3. 递归遍历右子树
}