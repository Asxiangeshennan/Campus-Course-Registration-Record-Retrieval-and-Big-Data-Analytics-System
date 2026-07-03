#include "data_manager.h"
#include "linked_list.h"
#include "avl_tree.h"
#include "hash_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* =========================================
 * 1. 全局变量
 * ========================================= */
// 当前激活的存储引擎，默认设置为三引擎全同步模式
StorageEngine g_active_engine = ENGINE_ALL_SYNC;
static LinkedList g_list;                  // 全局链表实例
static AVLTreeNode *g_avl_root = NULL;     // 全局AVL树根节点
static HashTable g_hash;                   // 全局哈希表实例
static char g_last_loaded_file[256] = {0}; // 记录最后一次加载的文件名
/* =========================================
 * 2. 内部辅助函数 (遍历与统计)
 * ========================================= */
// AVL树中序遍历收集数据，将树中所有节点的数据按顺序存入数组
static void AVL_CollectAll(AVLTreeNode *node, CourseRecord *records, int *index)
{
    if (!node)
        return;
    AVL_CollectAll(node->left, records, index);  // 遍历左子树
    records[(*index)++] = node->data;            // 保存当前节点数据
    AVL_CollectAll(node->right, records, index); // 遍历右子树
}
// 递归计算AVL树的节点总数
static int AVL_CountNodes(AVLTreeNode *node)
{
    if (!node)
        return 0;
    return 1 + AVL_CountNodes(node->left) + AVL_CountNodes(node->right);
}

// 获取当前激活引擎的所有记录 (核心内部函数)
// 根据当前选择的引擎，从对应的数据结构中提取所有数据到动态分配的数组中
static CourseRecord *GetAllRecords(int *count)
{
    *count = 0;
    CourseRecord *records = NULL;
    switch (g_active_engine)
    {
    case ENGINE_LINKED_LIST:
        *count = g_list.size;
        if (*count > 0)
        {
            records = (CourseRecord *)malloc((*count) * sizeof(CourseRecord));
            LinkedListNode *curr = g_list.head;
            int i = 0;
            while (curr && i < *count)
            {
                records[i++] = curr->data;
                curr = curr->next;
            }
        }
        break;
    case ENGINE_AVL_TREE:
        *count = AVL_CountNodes(g_avl_root);
        if (*count > 0)
        {
            records = (CourseRecord *)malloc((*count) * sizeof(CourseRecord));
            int index = 0;
            AVL_CollectAll(g_avl_root, records, &index);
        }
        break;
    case ENGINE_HASH_TABLE:
        *count = g_hash.size;
        if (*count > 0)
        {
            records = (CourseRecord *)malloc((*count) * sizeof(CourseRecord));
            int idx = 0;
            // 遍历哈希表的所有桶和链表
            for (int i = 0; i < HASH_TABLE_SIZE; i++)
            {
                HashNode *curr = g_hash.buckets[i];
                while (curr)
                {
                    if (idx < *count)
                        records[idx++] = curr->data;
                    curr = curr->next;
                }
            }
        }
        break;
    case ENGINE_ALL_SYNC:
    default:
        // 全同步模式下，默认从链表中读取数据（因为三引擎数据一致，取其一即可）
        *count = g_list.size;
        if (*count > 0)
        {
            records = (CourseRecord *)malloc((*count) * sizeof(CourseRecord));
            LinkedListNode *curr = g_list.head;
            int i = 0;
            while (curr && i < *count)
            {
                records[i++] = curr->data;
                curr = curr->next;
            }
        }
        break;
    }
    return records;
}

/* =========================================
 * 3. 初始化与销毁
 * ========================================= */
void DM_Init(void)
{
    List_Init(&g_list);
    g_avl_root = NULL;
    Hash_Init(&g_hash);
}

void DM_Destroy(void)
{
    List_Destroy(&g_list);
    AVL_Destroy(g_avl_root);
    g_avl_root = NULL;
    Hash_Destroy(&g_hash);
}

void DM_SetActiveEngine(StorageEngine engine)
{
    g_active_engine = engine;
}

/* =========================================
 * 4. 数据操作 (核心调度)
 * ========================================= */
// 三引擎同步插入记录，包含失败回滚机制，保证数据一致性
int DM_InsertRecord(const CourseRecord *rec)
{
    // 1. 插入链表
    if (!rec)
        return STATUS_ERR_NOT_FOUND;
    int s1 = List_Insert(&g_list, rec);
    if (s1 != STATUS_SUCCESS)
        return s1;
    // 2. 插入AVL树
    int s2 = 0;
    g_avl_root = AVL_Insert(g_avl_root, rec, &s2);
    if (s2 != STATUS_SUCCESS)
    {
        // 若AVL树插入失败，回滚链表操作
        List_DeleteByKey(&g_list, rec->student_id, rec->course_id);
        return s2;
    }
    // 3. 插入哈希表
    int s3 = Hash_Insert(&g_hash, rec);
    if (s3 != STATUS_SUCCESS)
    {
        // 若哈希表插入失败，回滚链表和AVL树操作
        List_DeleteByKey(&g_list, rec->student_id, rec->course_id);
        int rollback = 0;
        g_avl_root = AVL_Delete(g_avl_root, rec->student_id, rec->course_id, &rollback);
        return s3;
    }
    return STATUS_SUCCESS;
}
// 单引擎插入记录（根据当前激活的引擎执行），包含重修业务逻辑
int DM_InsertSingle(const CourseRecord *rec)
{
    if (!rec)
        return STATUS_ERR_NOT_FOUND;

    // 核心业务逻辑拦截：及格不重复，不及格可重修
    CourseRecord existing_rec;
    // 1. 尝试查找是否已经选过这门课
    if (DM_SearchRecord(rec->student_id, rec->course_id, &existing_rec) == STATUS_SUCCESS)
    {
        // 2. 如果存在，且原成绩已经及格 (>= 60)，则拒绝重复插入
        if (existing_rec.score >= 60)
        {
            return STATUS_ERR_DUPLICATE;
        }

        // 3. 如果存在但不及格 (< 60 或 -1)，说明是重修，必须先删除旧记录
        // 这样底层的 AVL/Hash 就不会因为“键重复”而拒绝新数据的插入
        DM_DeleteRecord(rec->student_id, rec->course_id);
    }

    // 4. 继续执行当前引擎的底层插入逻辑
    switch (g_active_engine)
    {
    case ENGINE_LINKED_LIST:
        return List_Insert(&g_list, rec);
    case ENGINE_AVL_TREE:
    {
        int s = 0;
        g_avl_root = AVL_Insert(g_avl_root, rec, &s);
        return (s == STATUS_SUCCESS) ? STATUS_SUCCESS : STATUS_ERR_DUPLICATE;
    }
    case ENGINE_HASH_TABLE:
        return Hash_Insert(&g_hash, rec);
    case ENGINE_ALL_SYNC:
        // 如果是全同步模式，则调用三引擎同步插入函数
        return DM_InsertRecord(rec);
    default:
        return STATUS_ERR_NOT_FOUND;
    }
}
// 三引擎同步删除记录
int DM_DeleteRecord(const char *stu_id, const char *cou_id)
{
    // 分别从三个数据结构中删除
    int s1 = List_DeleteByKey(&g_list, stu_id, cou_id);
    int s2 = 0;
    g_avl_root = AVL_Delete(g_avl_root, stu_id, cou_id, &s2);
    int s3 = Hash_Delete(&g_hash, stu_id, cou_id);
    // 只要有一个引擎删除成功，就认为整体成功
    if (s1 == STATUS_SUCCESS || s2 == STATUS_SUCCESS || s3 == STATUS_SUCCESS)
        return STATUS_SUCCESS;
    return STATUS_ERR_NOT_FOUND;
}
// 单引擎删除记录
int DM_DeleteSingle(const char *stu_id, const char *cou_id)
{
    switch (g_active_engine)
    {
    case ENGINE_LINKED_LIST:
        return List_DeleteByKey(&g_list, stu_id, cou_id);
    case ENGINE_AVL_TREE:
    {
        int s = 0;
        g_avl_root = AVL_Delete(g_avl_root, stu_id, cou_id, &s);
        return (s == STATUS_SUCCESS) ? STATUS_SUCCESS : STATUS_ERR_NOT_FOUND;
    }
    case ENGINE_HASH_TABLE:
        return Hash_Delete(&g_hash, stu_id, cou_id);
    case ENGINE_ALL_SYNC:
        return DM_DeleteRecord(stu_id, cou_id);
    default:
        return STATUS_ERR_NOT_FOUND;
    }
}

// 更新成绩：先查找原记录，修改分数后，先删后插实现更新
int DM_UpdateScore(const char *stu_id, const char *cou_id, int new_score)
{
    CourseRecord rec;
    if (DM_SearchRecord(stu_id, cou_id, &rec) != STATUS_SUCCESS)
        return STATUS_ERR_NOT_FOUND; // 找不到记录则更新失败
    rec.score = new_score;
    DM_DeleteRecord(stu_id, cou_id); // 删除旧记录
    return DM_InsertRecord(&rec);    // 插入新记录
}
// 根据当前激活的引擎查找特定记录
int DM_SearchRecord(const char *stu_id, const char *cou_id, CourseRecord *rec)
{
    if (!rec)
        return STATUS_ERR_NOT_FOUND;

    // ✅ 必须根据当前激活的引擎进行查找！
    switch (g_active_engine)
    {
    case ENGINE_LINKED_LIST:
    {
        LinkedListNode *lnode = List_SearchByKey(&g_list, stu_id, cou_id);
        if (lnode)
        {
            *rec = lnode->data;
            return STATUS_SUCCESS;
        }
        break;
    }
    case ENGINE_AVL_TREE:
    {
        AVLTreeNode *anode = AVL_Search(g_avl_root, stu_id, cou_id);
        if (anode)
        {
            *rec = anode->data;
            return STATUS_SUCCESS;
        }
        break;
    }
    case ENGINE_HASH_TABLE:
    {
        HashNode *hnode = Hash_Search(&g_hash, stu_id, cou_id);
        if (hnode)
        {
            *rec = hnode->data;
            return STATUS_SUCCESS;
        }
        break;
    }
    case ENGINE_ALL_SYNC:
    {
        // 全同步模式下，默认从链表查找（数据一致）
        LinkedListNode *lnode = List_SearchByKey(&g_list, stu_id, cou_id);
        if (lnode)
        {
            *rec = lnode->data;
            return STATUS_SUCCESS;
        }
        break;
    }
    }
    return STATUS_ERR_NOT_FOUND;
}
/* =========================================
 * 5. 筛选与持久化 (修复了CSV换行符问题)
 * ========================================= */
// 根据多条件筛选记录
CourseRecord *DM_FilterRecords(const FilterCriteria *criteria, int *count)
{
    int total = 0;
    CourseRecord *all = GetAllRecords(&total); // 获取所有记录
    if (!all || total == 0)
    {
        *count = 0;
        return NULL;
    }
    CourseRecord *results = (CourseRecord *)malloc(total * sizeof(CourseRecord));
    int res_count = 0;
    for (int i = 0; i < total; i++)
    {
        bool match = true;
        // 学号精确匹配
        if (criteria->student_id[0] != '\0' && strcmp(all[i].student_id, criteria->student_id) != 0)
            match = false;
        // 课程匹配（支持模糊匹配课程名或精确匹配课程号）
        if (match && criteria->course_id[0] != '\0')
        {
            if (criteria->is_fuzzy_course)
            {
                if (strstr(all[i].course_name, criteria->course_name) == NULL)
                    match = false;
            }
            else
            {
                if (strcmp(all[i].course_id, criteria->course_id) != 0)
                    match = false;
            }
        }
        // 学期精确匹配
        if (match && criteria->semester[0] != '\0' && strcmp(all[i].semester, criteria->semester) != 0)
            match = false;
        // 学院精确匹配
        if (match && criteria->college[0] != '\0' && strcmp(all[i].college, criteria->college) != 0)
            match = false;
        // 分数范围匹配（-1表示未出分，不参与分数筛选）
        if (match && all[i].score != -1 && (all[i].score < criteria->score_min || all[i].score > criteria->score_max))
            match = false;
        if (match)
            results[res_count++] = all[i];
    }
    free(all);
    *count = res_count;
    return results;
}
// 将数据保存到CSV文件
int DM_SaveToFile(const char *filename)
{
    int count = 0;
    CourseRecord *all = GetAllRecords(&count);
    if (!all || count == 0)
        return 0;
    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        free(all);
        return -1;
    }
    // 写入CSV表头
    fprintf(fp, "student_id,name,college,course_id,course_name,credit,semester,enroll_date,score,is_elective\n");
    // 逐条写入记录
    for (int i = 0; i < count; i++)
    {
        fprintf(fp, "%s,%s,%s,%s,%s,%.1f,%s,%s,%d,%d\n",
                all[i].student_id, all[i].name, all[i].college,
                all[i].course_id, all[i].course_name, all[i].credit,
                all[i].semester, all[i].enroll_date, all[i].score, all[i].is_elective);
    }
    fclose(fp);
    free(all);
    return count;
}
// 从CSV文件加载数据
int DM_LoadFromFile(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
        return -1;
    // ★ 新增：记录当前加载的文件名，供退出时默认保存使用
    SafeStrCopy(g_last_loaded_file, sizeof(g_last_loaded_file), filename);
    char line[512];
    int count = 0;
    // 读取并跳过表头
    if (fgets(line, sizeof(line), fp) == NULL)
    {
        fclose(fp);
        return 0;
    } // 跳过表头
    while (fgets(line, sizeof(line), fp))
    {
        CourseRecord rec = {0};
        // 使用 strtok 解析CSV，分隔符包含逗号、换行符(\n)和回车符(\r)，防止跨平台换行导致解析错误
        char *token = strtok(line, ",\n\r");
        if (!token)
            continue;
        SafeStrCopy(rec.student_id, MAX_STU_ID_LEN, token);
        if ((token = strtok(NULL, ",\n\r")))
            SafeStrCopy(rec.name, MAX_NAME_LEN, token);
        if ((token = strtok(NULL, ",\n\r")))
            SafeStrCopy(rec.college, MAX_COLLEGE_LEN, token);
        if ((token = strtok(NULL, ",\n\r")))
            SafeStrCopy(rec.course_id, MAX_COU_ID_LEN, token);
        if ((token = strtok(NULL, ",\n\r")))
            SafeStrCopy(rec.course_name, MAX_COU_NAME_LEN, token);
        if ((token = strtok(NULL, ",\n\r")))
            rec.credit = atof(token);
        if ((token = strtok(NULL, ",\n\r")))
            SafeStrCopy(rec.semester, MAX_SEMESTER_LEN, token);
        if ((token = strtok(NULL, ",\n\r")))
            SafeStrCopy(rec.enroll_date, MAX_DATE_LEN, token);
        if ((token = strtok(NULL, ",\n\r")))
            rec.score = atoi(token);
        if ((token = strtok(NULL, ",\n\r")))
            rec.is_elective = atoi(token);
        // 调用三引擎同步插入，保证加载的数据在所有结构中一致
        if (DM_InsertRecord(&rec) == STATUS_SUCCESS)
            count++;
    }
    fclose(fp);
    return count;
}

int DM_CleanExpiredRecords(void)
{
    // 简单实现：目前返回0，表示没有过期记录需要清理
    return 0;
}

int DM_GetDataSize(void)
{
    switch (g_active_engine)
    {
    case ENGINE_LINKED_LIST:
        return g_list.size;
    case ENGINE_AVL_TREE:
        return AVL_CountNodes(g_avl_root);
    case ENGINE_HASH_TABLE:
        return g_hash.size;
    case ENGINE_ALL_SYNC:
        return g_list.size;
    default:
        return 0;
    }
}

/* =========================================
 * 6. 统计分析
 * ========================================= */
// 获取课程选课统计（选课人数、使用率）
CourseEnrollmentStat *DM_GetCourseEnrollmentStats(int *count)
{
    int total = 0;
    CourseRecord *all = GetAllRecords(&total);
    if (!all || total == 0)
    {
        *count = 0;
        return NULL;
    }
    CourseEnrollmentStat *stats = (CourseEnrollmentStat *)calloc(total, sizeof(CourseEnrollmentStat));
    int stat_count = 0;
    // 聚合统计每门课的选课人数
    for (int i = 0; i < total; i++)
    {
        bool found = false;
        for (int j = 0; j < stat_count; j++)
        {
            if (strcmp(stats[j].course_id, all[i].course_id) == 0)
            {
                stats[j].enroll_count++;
                found = true;
                break;
            }
        }
        if (!found)
        {
            strncpy(stats[stat_count].course_id, all[i].course_id, MAX_COU_ID_LEN - 1);
            strncpy(stats[stat_count].course_name, all[i].course_name, MAX_COU_NAME_LEN - 1);
            stats[stat_count].enroll_count = 1;
            stat_count++;
        }
    }
    // 计算使用率 (假设每门课最大容量为180人)
    for (int i = 0; i < stat_count; i++)
        stats[i].usage_rate = (stats[i].enroll_count / 200.0f) * 100.0f;

    free(all);
    *count = stat_count;
    return stats;
}
// 获取学生学分统计（选课数、已获得总学分）
StudentCreditStat *DM_GetStudentCreditStats(int *count)
{
    int total = 0;
    CourseRecord *all = GetAllRecords(&total);
    if (!all || total == 0)
    {
        *count = 0;
        return NULL;
    }
    StudentCreditStat *stats = (StudentCreditStat *)calloc(total, sizeof(StudentCreditStat));
    int stat_count = 0;
    for (int i = 0; i < total; i++)
    {
        bool found = false;
        for (int j = 0; j < stat_count; j++)
        {
            if (strcmp(stats[j].student_id, all[i].student_id) == 0)
            {
                stats[j].course_count++;
                // 只有成绩及格(>=60)的课程才计入学分
                if (all[i].score != -1 && all[i].score >= 60)
                    stats[j].total_credit += all[i].credit;
                found = true;
                break;
            }
        }
        if (!found)
        {
            strncpy(stats[stat_count].student_id, all[i].student_id, MAX_STU_ID_LEN - 1);
            strncpy(stats[stat_count].name, all[i].name, MAX_NAME_LEN - 1);
            stats[stat_count].course_count = 1;
            stats[stat_count].total_credit = (all[i].score != -1 && all[i].score >= 60) ? all[i].credit : 0;
            stat_count++;
        }
    }
    free(all);
    *count = stat_count;
    return stats;
}
// 获取学院分布统计（各学院选课人数及占比）
CollegeDistributionStat *DM_GetCollegeDistributionStats(int *count)
{
    int total = 0;
    CourseRecord *all = GetAllRecords(&total);
    if (!all || total == 0)
    {
        *count = 0;
        return NULL;
    }
    CollegeDistributionStat *stats = (CollegeDistributionStat *)calloc(total, sizeof(CollegeDistributionStat));
    int stat_count = 0;
    for (int i = 0; i < total; i++)
    {
        bool found = false;
        for (int j = 0; j < stat_count; j++)
        {
            if (strcmp(stats[j].college, all[i].college) == 0)
            {
                stats[j].enroll_count++;
                found = true;
                break;
            }
        }
        if (!found)
        {
            strncpy(stats[stat_count].college, all[i].college, MAX_COLLEGE_LEN - 1);
            stats[stat_count].enroll_count = 1;
            stat_count++;
        }
    }
    // 计算百分比
    for (int i = 0; i < stat_count; i++)
        stats[i].percentage = (stats[i].enroll_count * 100.0f) / total;
    free(all);
    *count = stat_count;
    return stats;
}
// 获取学期趋势统计（各学期选课总人次）
SemesterTrendStat *DM_GetSemesterTrendStats(int *count)
{
    int total = 0;
    CourseRecord *all = GetAllRecords(&total);
    if (!all || total == 0)
    {
        *count = 0;
        return NULL;
    }
    SemesterTrendStat *stats = (SemesterTrendStat *)calloc(total, sizeof(SemesterTrendStat));
    int stat_count = 0;
    for (int i = 0; i < total; i++)
    {
        bool found = false;
        for (int j = 0; j < stat_count; j++)
        {
            if (strcmp(stats[j].semester, all[i].semester) == 0)
            {
                stats[j].total_students++;
                found = true;
                break;
            }
        }
        if (!found)
        {
            strncpy(stats[stat_count].semester, all[i].semester, MAX_SEMESTER_LEN - 1);
            stats[stat_count].total_students = 1;
            stat_count++;
        }
    }
    free(all);
    *count = stat_count;
    return stats;
}
// 获取成绩分布统计（优秀、良好、中等、及格、不及格、待出分）
void DM_GetScoreDistributionStats(ScoreDistributionStat *stat)
{
    if (!stat)
        return;
    memset(stat, 0, sizeof(ScoreDistributionStat));
    int total = 0;
    CourseRecord *all = GetAllRecords(&total);
    if (!all)
        return;
    for (int i = 0; i < total; i++)
    {
        stat->total++;
        if (all[i].score == -1)
            stat->pending++;
        else if (all[i].score >= 90)
            stat->excellent++;
        else if (all[i].score >= 80)
            stat->good++;
        else if (all[i].score >= 70)
            stat->medium++;
        else if (all[i].score >= 60)
            stat->pass++;
        else
            stat->fail++;
    }
    free(all);
}
// 估算当前激活引擎的内存使用量（字节）
long long DM_GetMemoryUsage(void)
{
    int count = DM_GetDataSize();
    switch (g_active_engine)
    {
    case ENGINE_LINKED_LIST:
        // 节点大小 * 数量 + 链表头结构大小
        return (long long)count * sizeof(LinkedListNode) + sizeof(LinkedList);
    case ENGINE_AVL_TREE:
        // 节点大小 * 数量
        return (long long)count * sizeof(AVLTreeNode);
    case ENGINE_HASH_TABLE:
        // 桶数组大小 + 节点大小 * 数量
        return (long long)HASH_TABLE_SIZE * sizeof(HashNode *) + (long long)count * sizeof(HashNode);
    default:
        return 0;
    }
}
// 纯插入函数，绕过业务查重逻辑（用于初始化或特殊场景）
int DM_InsertPure(const CourseRecord *rec)
{
    if (!rec)
        return STATUS_ERR_NOT_FOUND;

    // 直接根据当前引擎执行底层插入，不做 DM_SearchRecord 查重
    switch (g_active_engine)
    {
    case ENGINE_LINKED_LIST:
        return List_Insert(&g_list, rec);
    case ENGINE_AVL_TREE:
    {
        int s = 0;
        g_avl_root = AVL_Insert(g_avl_root, rec, &s);
        return (s == STATUS_SUCCESS) ? STATUS_SUCCESS : STATUS_ERR_DUPLICATE;
    }
    case ENGINE_HASH_TABLE:
        return Hash_Insert(&g_hash, rec);
    case ENGINE_ALL_SYNC:
        return DM_InsertRecord(rec);
    default:
        return STATUS_ERR_NOT_FOUND;
    }
}
/* =========================================
 * 7. 自适应智能插入策略 (Adaptive Insertion)
 * ========================================= */
#define ADAPTIVE_THRESHOLD 5000 // 自适应阈值：5000条
// 自适应智能插入策略
// 核心思想：小数据量时保证数据冗余和稳定性，大数据量时追求极致性能
int DM_AdaptiveInsert(const CourseRecord *rec)
{
    if (!rec)
        return STATUS_ERR_NOT_FOUND;

    int current_size = DM_GetDataSize();

    if (current_size < ADAPTIVE_THRESHOLD)
    {
        // 【策略A】数据量 < 5000：三引擎全同步，保证稳定性与数据冗余
        g_active_engine = ENGINE_ALL_SYNC;
        return DM_InsertRecord(rec); // 强制写三个引擎
    }
    else
    {
        // 【策略B】数据量 >= 5000：切换为哈希表单引擎，追求极致时间性能 O(1)
        g_active_engine = ENGINE_HASH_TABLE;
        return DM_InsertSingle(rec); // 根据当前引擎(哈希)执行，并自动处理重修业务逻辑
    }
}
const char *DM_GetLastLoadedFile(void)
{
    return g_last_loaded_file;
}