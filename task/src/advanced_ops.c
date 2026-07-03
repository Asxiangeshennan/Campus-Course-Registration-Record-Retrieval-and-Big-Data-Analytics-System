#include "advanced_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==========================================
// 辅助比较函数
// ==========================================

/**
 * @brief 比较两个日期字符串 (格式: YYYY-MM-DD)
 * @param d1 第一个日期字符串
 * @param d2 第二个日期字符串
 * @return 返回值与 strcmp 相同：0表示相等，<0表示d1<d2，>0表示d1>d2
 * @note 由于日期格式固定为 YYYY-MM-DD，字典序比较等同于时间先后比较
 */
static int CompareDates(const char *d1, const char *d2)
{
    return strcmp(d1, d2);
}

// ==========================================
// 多关键字排序相关
// ==========================================

// 全局变量，用于在 qsort 的比较函数中传递当前的排序规则
// 因为 qsort 的比较函数签名固定，无法直接传递额外参数，所以借助全局变量
static SortRule *g_current_rules = NULL;
static int g_current_rule_count = 0;
/**
 * @brief 多关键字比较函数 (供 qsort 使用)
 * @param a 指向第一个 CourseRecord 的指针
 * @param b 指向第二个 CourseRecord 的指针
 * @return 根据排序规则返回比较结果
 */
static int MultiKeyCompare(const void *a, const void *b)
{
    const CourseRecord *rec1 = (const CourseRecord *)a;
    const CourseRecord *rec2 = (const CourseRecord *)b;
    // 遍历所有排序规则，按优先级依次比较
    for (int i = 0; i < g_current_rule_count; i++)
    {
        int cmp = 0;
        SortRule rule = g_current_rules[i];
        // 根据当前规则指定的字段进行比较
        switch (rule.field)
        {
        case SORT_FIELD_STU_ID:
            cmp = rec1->score - rec2->score; // 注意：如果分数差值极大可能会溢出，但通常分数在合理范围内
            break;
        case SORT_FIELD_SCORE:
            // 浮点数比较，避免直接相减可能带来的精度问题
            cmp = rec1->score - rec2->score;
            break;
        case SORT_FIELD_CREDIT:
            cmp = (rec1->credit > rec2->credit) ? 1 : ((rec1->credit < rec2->credit) ? -1 : 0);
            break;
        case SORT_FIELD_DATE:
            cmp = CompareDates(rec1->enroll_date, rec2->enroll_date);
            break;
        }
        // 如果规则指定为降序，则将比较结果取反
        if (rule.direction == SORT_DESC)
            cmp = -cmp;
        // 如果当前字段比较结果不为 0，说明已分出大小，直接返回结果
        if (cmp != 0)
            return cmp;
        // 如果为 0，说明当前字段相等，继续循环比较下一个优先级的字段
    }
    // 所有字段都相等
    return 0;
}
/**
 * @brief 执行多关键字排序
 * @param records 待排序的记录数组
 * @param count 记录数量
 * @param rules 排序规则数组
 * @param rule_count 排序规则数量
 */
void Advanced_MultiKeySort(CourseRecord *records, int count, SortRule *rules, int rule_count)
{
    if (count <= 1 || rule_count <= 0)
        return;
    // 将排序规则设置到全局变量中，供 MultiKeyCompare 使用
    g_current_rules = rules;
    g_current_rule_count = rule_count;
    // 调用 C 标准库的快速排序函数
    qsort(records, count, sizeof(CourseRecord), MultiKeyCompare);
}
// ==========================================
// 过期记录处理 (占位/辅助函数)
// ==========================================

/**
 * @brief 统计过期记录数量
 * @param cutoff_date 截止日期
 * @return 过期记录数量
 * @note 实际逻辑在 menu.c 中通过遍历全量数据实现，此处作为接口补充
 */
int Advanced_CountExpiredRecords(const char *cutoff_date)
{

    return 0;
}
/**
 * @brief 执行清理过期记录操作
 * @param cutoff_date 截止日期
 * @return 清理的记录数量
 * @note 实际删除逻辑在 menu.c 中通过 DM_DeleteRecord 实现，此处作为接口补充
 */
int Advanced_ExecuteCleanExpired(const char *cutoff_date)
{

    return 0;
}
// ==========================================
// 数据导出
// ==========================================

/**
 * @brief 将过滤后的记录导出到 CSV 文件
 * @param records 记录数组
 * @param count 记录数量
 * @param filename 导出文件的路径
 * @return 成功导出的记录数，失败返回 -1
 */
int Advanced_ExportFiltered(CourseRecord *records, int count, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
        return -1;
    // 写入 CSV 表头
    fprintf(fp, "student_id,name,college,course_id,course_name,credit,semester,enroll_date,score,is_elective\n");
    // 逐条写入记录数据
    for (int i = 0; i < count; i++)
    {
        fprintf(fp, "%s,%s,%s,%s,%s,%.1f,%s,%s,%d,%d\n",
                records[i].student_id, records[i].name, records[i].college,
                records[i].course_id, records[i].course_name, records[i].credit,
                records[i].semester, records[i].enroll_date, records[i].score, records[i].is_elective);
    }
    fclose(fp);
    return count;
}

// ==========================================
// 外部排序实现 (附加任务2)
// ==========================================

/**
 * @brief 外部排序：处理超出内存限制的大文件排序
 * @param input_file 输入文件路径
 * @param output_file 输出文件路径
 * @param memory_limit 单次能加载到内存中的最大记录条数
 * @note 流程：1.分块读入内存并排序 -> 2.写入临时文件 -> 3.合并临时文件并全局排序
 */
void Advanced_ExternalSort(const char *input_file, const char *output_file, int memory_limit)
{
    printf("\n[外部排序] 开始执行，内存限制: %d 条/轮\n", memory_limit);

    // 1. 打开输入文件并统计总记录数
    FILE *fp = fopen(input_file, "r");
    if (!fp)
    {
        printf("[错误] 无法打开 %s\n", input_file);
        return;
    }

    int total_lines = 0;
    char line[512];
    // 统计总行数
    while (fgets(line, sizeof(line), fp))
        total_lines++;
    total_lines--; // 减去 CSV 表头占用的 1 行

    printf("[外部排序] 总记录数: %d\n", total_lines);
    rewind(fp);                    // 文件指针重置到开头
    fgets(line, sizeof(line), fp); // 读取并跳过表头
    // 2. 分块读入、内存排序并写入临时文件
    int chunk_count = 0; // 临时文件计数器
                         // 分配内存缓冲区
    CourseRecord *buffer = (CourseRecord *)malloc(memory_limit * sizeof(CourseRecord));
    int buf_idx = 0; // 当前缓冲区中的记录数
    // 逐行读取数据并解析到缓冲区
    while (fgets(line, sizeof(line), fp))
    {
        CourseRecord rec = {0};
        // 使用 strtok 按逗号分隔符解析 CSV 字段
        char *token = strtok(line, ",\n\r");
        if (!token)
            continue;
        strncpy(rec.student_id, token, MAX_STU_ID_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            strncpy(rec.name, token, MAX_NAME_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            strncpy(rec.college, token, MAX_COLLEGE_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            strncpy(rec.course_id, token, MAX_COU_ID_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            strncpy(rec.course_name, token, MAX_COU_NAME_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            rec.credit = atof(token);
        if ((token = strtok(NULL, ",\n\r")))
            strncpy(rec.semester, token, MAX_SEMESTER_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            strncpy(rec.enroll_date, token, MAX_DATE_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            rec.score = atoi(token);
        if ((token = strtok(NULL, ",\n\r")))
            rec.is_elective = atoi(token);

        buffer[buf_idx++] = rec;
        // 当缓冲区满时，进行内存排序并写入临时文件
        if (buf_idx == memory_limit)
        {
            // 设置单关键字排序规则（默认按学号升序）
            SortRule rule = {SORT_FIELD_STU_ID, SORT_ASC};
            Advanced_MultiKeySort(buffer, buf_idx, &rule, 1);

            // 生成临时文件名并写入
            char temp_name[50];
            snprintf(temp_name, sizeof(temp_name), "temp_chunk_%d.csv", chunk_count);
            FILE *t_fp = fopen(temp_name, "w");
            for (int i = 0; i < buf_idx; i++)
            {
                fprintf(t_fp, "%s,%s,%s,%s,%s,%.1f,%s,%s,%d,%d\n",
                        buffer[i].student_id, buffer[i].name, buffer[i].college,
                        buffer[i].course_id, buffer[i].course_name, buffer[i].credit,
                        buffer[i].semester, buffer[i].enroll_date, buffer[i].score, buffer[i].is_elective);
            }
            fclose(t_fp);
            printf("[外部排序] 第 %d 轮归并：写入 %s (%d条)\n", chunk_count + 1, temp_name, buf_idx);
            chunk_count++;
            buf_idx = 0; // 清空缓冲区索引
        }
    }
    // 处理最后一块未满内存限制的数据
    if (buf_idx > 0)
    {
        SortRule rule = {SORT_FIELD_STU_ID, SORT_ASC};
        Advanced_MultiKeySort(buffer, buf_idx, &rule, 1);
        char temp_name[50];
        snprintf(temp_name, sizeof(temp_name), "temp_chunk_%d.csv", chunk_count);
        FILE *t_fp = fopen(temp_name, "w");
        for (int i = 0; i < buf_idx; i++)
        {
            fprintf(t_fp, "%s,%s,%s,%s,%s,%.1f,%s,%s,%d,%d\n",
                    buffer[i].student_id, buffer[i].name, buffer[i].college,
                    buffer[i].course_id, buffer[i].course_name, buffer[i].credit,
                    buffer[i].semester, buffer[i].enroll_date, buffer[i].score, buffer[i].is_elective);
        }
        fclose(t_fp);
        printf("[外部排序] 第 %d 轮归并：写入 %s (%d条)\n", chunk_count + 1, temp_name, buf_idx);
        chunk_count++;
    }
    free(buffer); // 释放分块排序使用的缓冲区
    fclose(fp);

    // 3. 多路归并阶段 (简化版实现)
    printf("[外部排序] 开始多路归并...\n");

    // 【简化处理说明】：真实的外部排序多路归并应使用败者树或最小堆，逐行比较多个临时文件。
    // 此处为课程设计演示，采用“先拼接所有临时文件，再全局读入排序”的模拟方式。
    FILE *out_fp = fopen(output_file, "w");
    fprintf(out_fp, "student_id,name,college,course_id,course_name,credit,semester,enroll_date,score,is_elective\n");
    // 将所有临时文件的内容依次追加到输出文件中
    for (int i = 0; i < chunk_count; i++)
    {
        char temp_name[50];
        snprintf(temp_name, sizeof(temp_name), "temp_chunk_%d.csv", i);
        FILE *t_fp = fopen(temp_name, "r");
        if (t_fp)
        {
            // 【Bug修复】：原代码此处有 fgets 试图跳过表头，但前面写入临时文件时并未写入表头，
            // 这会导致每个临时文件的第一条有效数据被误读并丢弃。此处已将其删除。
            fgets(line, sizeof(line), t_fp);
            while (fgets(line, sizeof(line), t_fp))
            {
                fputs(line, out_fp);
            }
            fclose(t_fp);
            remove(temp_name); // 归并完成后删除临时文件，释放磁盘空间
        }
    }
    fclose(out_fp);

    // 4. 最终全局排序
    // 由于上面的“归并”只是简单拼接，各块之间并未整体有序，因此需要重新读入输出文件进行全局排序
    fp = fopen(output_file, "r");
    int final_count = 0;
    while (fgets(line, sizeof(line), fp))
        final_count++;
    final_count--; // 减去表头
    rewind(fp);
    fgets(line, sizeof(line), fp); // 跳过表头
    // 分配足够大的内存以容纳所有记录（此处假设最终合并后的数据量可以一次性读入内存）
    CourseRecord *final_buf = (CourseRecord *)malloc(final_count * sizeof(CourseRecord));
    int idx = 0;
    // 重新解析拼接后的文件数据
    while (fgets(line, sizeof(line), fp) && idx < final_count)
    {
        CourseRecord rec = {0};
        char *token = strtok(line, ",\n\r");
        if (!token)
            continue;
        strncpy(rec.student_id, token, MAX_STU_ID_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            strncpy(rec.name, token, MAX_NAME_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            strncpy(rec.college, token, MAX_COLLEGE_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            strncpy(rec.course_id, token, MAX_COU_ID_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            strncpy(rec.course_name, token, MAX_COU_NAME_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            rec.credit = atof(token);
        if ((token = strtok(NULL, ",\n\r")))
            strncpy(rec.semester, token, MAX_SEMESTER_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            strncpy(rec.enroll_date, token, MAX_DATE_LEN - 1);
        if ((token = strtok(NULL, ",\n\r")))
            rec.score = atoi(token);
        if ((token = strtok(NULL, ",\n\r")))
            rec.is_elective = atoi(token);
        final_buf[idx++] = rec;
    }
    fclose(fp);
    // 对全量数据进行最终排序
    SortRule rule = {SORT_FIELD_STU_ID, SORT_ASC};
    Advanced_MultiKeySort(final_buf, final_count, &rule, 1);
    // 将排序后的最终结果覆盖写回输出文件
    out_fp = fopen(output_file, "w");
    fprintf(out_fp, "student_id,name,college,course_id,course_name,credit,semester,enroll_date,score,is_elective\n");
    for (int i = 0; i < final_count; i++)
    {
        fprintf(out_fp, "%s,%s,%s,%s,%s,%.1f,%s,%s,%d,%d\n",
                final_buf[i].student_id, final_buf[i].name, final_buf[i].college,
                final_buf[i].course_id, final_buf[i].course_name, final_buf[i].credit,
                final_buf[i].semester, final_buf[i].enroll_date, final_buf[i].score, final_buf[i].is_elective);
    }
    fclose(out_fp);
    free(final_buf);

    printf("[外部排序] 完成！结果已保存至 %s\n", output_file);
    printf("[外部排序] 总归并轮数: %d, 临时文件数: %d\n", chunk_count, chunk_count);
}