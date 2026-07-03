#include "external_sort.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 宏定义配置 ==================== */
#define MEMORY_LIMIT 1000    // 内存最多容纳的记录数（即每次在内存中排序的顺串大小）
#define K_WAY 10             // 每轮归并路数（即每次同时合并的临时文件数量）
#define MAX_TEMP_FILES 200   // 临时文件数组的最大容量
#define MAX_FILENAME_LEN 128 // 文件名最大长度限制

/* ==================== 工具函数 ==================== */

/**
 * @brief qsort 比较函数：按 student_id 字典序升序排列
 */
static int CompareRecordByID(const void *a, const void *b)
{
    const CourseRecord *ra = (const CourseRecord *)a;
    const CourseRecord *rb = (const CourseRecord *)b;
    return strcmp(ra->student_id, rb->student_id);
}

/**
 * @brief 解析单行 CSV 数据到 CourseRecord 结构体中
 * @param line 原始 CSV 行字符串
 * @param rec  输出的结构体指针
 * @return 是否成功解析出 10 个字段
 */
static bool ParseCSVLine(const char *line, CourseRecord *rec)
{
    char buffer[512];
    // 拷贝到局部缓冲区，避免修改原字符串
    SafeStrCopy(buffer, sizeof(buffer), line);

    char *ptr = buffer;
    char *comma;
    int field_idx = 0;

    // 逐个字段解析，最多解析 10 个字段
    while (ptr && field_idx < 10)
    {
        comma = strchr(ptr, ','); // 查找下一个逗号
        if (comma)
            *comma = '\0'; // 截断当前字段

        // 去除字段末尾可能存在的回车/换行符 (\r, \n)
        char *end = ptr + strlen(ptr) - 1;
        while (end >= ptr && (*end == '\r' || *end == '\n'))
            *end-- = '\0';

        // 根据字段索引赋值到结构体对应成员
        switch (field_idx)
        {
        case 0:
            SafeStrCopy(rec->student_id, MAX_STU_ID_LEN, ptr);
            break;
        case 1:
            SafeStrCopy(rec->name, MAX_NAME_LEN, ptr);
            break;
        case 2:
            SafeStrCopy(rec->college, MAX_COLLEGE_LEN, ptr);
            break;
        case 3:
            SafeStrCopy(rec->course_id, MAX_COU_ID_LEN, ptr);
            break;
        case 4:
            SafeStrCopy(rec->course_name, MAX_COU_NAME_LEN, ptr);
            break;
        case 5:
            rec->credit = atof(ptr);
            break;
        case 6:
            SafeStrCopy(rec->semester, MAX_SEMESTER_LEN, ptr);
            break;
        case 7:
            SafeStrCopy(rec->enroll_date, MAX_DATE_LEN, ptr);
            break;
        case 8:
            rec->score = atoi(ptr);
            break;
        case 9:
            rec->is_elective = atoi(ptr);
            break;
        }
        field_idx++;

        if (!comma)
            break;       // 如果没有逗号了，说明是最后一个字段，退出循环
        ptr = comma + 1; // 移动指针到下一个字段的起始位置
    }
    return field_idx >= 10; // 确保成功解析了 10 个字段
}

/**
 * @brief 从 CSV 行中快速提取 student_id (用于构建最小堆，减少内存和比较开销)
 * @param line    原始 CSV 行字符串
 * @param id_out  输出的 student_id 缓冲区
 * @param id_size 缓冲区大小
 */
static void ExtractStudentID(const char *line, char *id_out, int id_size)
{
    const char *comma = strchr(line, ',');
    int len = comma ? (int)(comma - line) : (int)strlen(line);
    if (len >= id_size)
        len = id_size - 1;

    strncpy(id_out, line, len);
    id_out[len] = '\0';

    // 去除末尾的换行符
    char *end = id_out + strlen(id_out) - 1;
    while (end >= id_out && (*end == '\r' || *end == '\n'))
        *end-- = '\0';
}

/**
 * @brief 将 CourseRecord 格式化为一行 CSV 并写入文件
 */
static void WriteRecord(FILE *f, const CourseRecord *r)
{
    fprintf(f, "%s,%s,%s,%s,%s,%.1f,%s,%s,%d,%d\n",
            r->student_id, r->name, r->college,
            r->course_id, r->course_name, r->credit,
            r->semester, r->enroll_date, r->score, r->is_elective);
}

/* ==================== 最小堆（用于 K 路归并） ==================== */

/**
 * @brief 最小堆节点结构体
 */
typedef struct
{
    char student_id[MAX_STU_ID_LEN]; // 用于堆排序比较的键值
    char line[1024];                 // 完整的 CSV 行数据，用于直接写入输出文件
    int file_idx;                    // 记录该节点来自哪个输入文件，以便后续继续读取
} HeapNode;

/**
 * @brief 向最小堆中插入节点 (上浮操作)
 */
static void HeapPush(HeapNode *heap, int *size, HeapNode node)
{
    int i = (*size)++;
    heap[i] = node;

    // 与父节点比较，若小于父节点则交换，直到满足最小堆性质
    while (i > 0)
    {
        int parent = (i - 1) / 2;
        if (strcmp(heap[i].student_id, heap[parent].student_id) < 0)
        {
            HeapNode tmp = heap[i];
            heap[i] = heap[parent];
            heap[parent] = tmp;
            i = parent;
        }
        else
            break;
    }
}

/**
 * @brief 弹出最小堆的堆顶节点 (下沉操作)
 */
static HeapNode HeapPop(HeapNode *heap, int *size)
{
    HeapNode top = heap[0];    // 保存堆顶（最小值）
    heap[0] = heap[--(*size)]; // 将最后一个节点移到堆顶

    int i = 0;
    // 与子节点比较，向下调整以恢复最小堆性质
    while (1)
    {
        int l = 2 * i + 1, r = 2 * i + 2, min = i;
        if (l < *size && strcmp(heap[l].student_id, heap[min].student_id) < 0)
            min = l;
        if (r < *size && strcmp(heap[r].student_id, heap[min].student_id) < 0)
            min = r;

        if (min == i)
            break; // 已经满足最小堆性质，退出循环

        HeapNode tmp = heap[i];
        heap[i] = heap[min];
        heap[min] = tmp;
        i = min;
    }
    return top;
}

/* ==================== 阶段一：分割 (生成初始顺串) ==================== */

/**
 * @brief 外部排序第一阶段：分块读取、内存排序、写入临时文件
 */
static int SplitPhase(const char *input_file,
                      char names[][MAX_FILENAME_LEN], int *count_out,
                      long *read_ops, long *write_ops)
{
    FILE *fin = fopen(input_file, "r");
    if (!fin)
    {
        printf("[错误] 无法打开 %s\n", input_file);
        return -1;
    }

    char line[1024];
    fgets(line, sizeof(line), fin); // 读取并跳过 CSV 表头

    CourseRecord buffer[MEMORY_LIMIT]; // 内存缓冲区，用于存放当前批次的数据
    int count = 0, file_count = 0;
    *read_ops = *write_ops = 0;

    printf("\n========== 阶段一：分割 ==========\n");
    printf("  内存限制: %d 条/批\n", MEMORY_LIMIT);

    // 逐行读取数据
    while (fgets(line, sizeof(line), fin))
    {
        CourseRecord rec = {0};
        if (!ParseCSVLine(line, &rec))
            continue; // 解析失败的行直接跳过

        buffer[count++] = rec;
        (*read_ops)++;

        // 当缓冲区达到内存限制时，进行排序并写入临时文件
        if (count == MEMORY_LIMIT)
        {
            qsort(buffer, count, sizeof(CourseRecord), CompareRecordByID);
            snprintf(names[file_count], MAX_FILENAME_LEN, "temp_r0_%03d.csv", file_count);
            FILE *ft = fopen(names[file_count], "w");
            if (!ft)
            {
                fclose(fin);
                return -1;
            }
            for (int i = 0; i < count; i++)
            {
                WriteRecord(ft, &buffer[i]);
                (*write_ops)++;
            }
            fclose(ft);
            printf("  -> 生成 temp_r0_%03d.csv (累计读取 %ld 条)\n", file_count, *read_ops);
            file_count++;
            count = 0; // 清空缓冲区计数器
        }
    }

    // 处理文件末尾不足 MEMORY_LIMIT 条的剩余数据
    if (count > 0)
    {
        qsort(buffer, count, sizeof(CourseRecord), CompareRecordByID);
        snprintf(names[file_count], MAX_FILENAME_LEN, "temp_r0_%03d.csv", file_count);
        FILE *ft = fopen(names[file_count], "w");
        for (int i = 0; i < count; i++)
        {
            WriteRecord(ft, &buffer[i]);
            (*write_ops)++;
        }
        fclose(ft);
        file_count++;
    }
    fclose(fin);

    printf("  -------------------------------\n");
    printf("  分割完成: 读取 %ld, 写入 %ld, 临时文件 %d 个\n",
           *read_ops, *write_ops, file_count);
    *count_out = file_count;
    return 0;
}

/* ==================== 阶段二：单轮 K 路归并 ==================== */

/**
 * @brief 外部排序第二阶段：执行一轮 K 路归并
 */
static int MergeRound(char in_names[][MAX_FILENAME_LEN], int in_count,
                      char out_names[][MAX_FILENAME_LEN], int *out_count_out,
                      long *read_ops, long *write_ops, int round)
{
    *read_ops = *write_ops = 0;
    int out_count = 0, idx = 0;

    printf("\n========== 第 %d 轮归并 (k = %d) ==========\n", round, K_WAY);
    printf("  输入文件: %d 个\n", in_count);

    // 每次取 K_WAY 个文件进行归并，直到处理完所有输入文件
    while (idx < in_count)
    {
        // 计算当前批次实际需要合并的文件数（可能不足 K_WAY）
        int batch = (in_count - idx < K_WAY) ? (in_count - idx) : K_WAY;

        FILE **files = malloc(batch * sizeof(FILE *));
        HeapNode *heap = malloc(batch * sizeof(HeapNode));
        int heap_size = 0;

        // 初始化：从当前批次的每个文件中读取首行，插入最小堆
        for (int i = 0; i < batch; i++)
        {
            files[i] = fopen(in_names[idx + i], "r");
            if (!files[i])
                return -1;

            HeapNode node;
            node.file_idx = i;
            if (fgets(node.line, sizeof(node.line), files[i]))
            {
                ExtractStudentID(node.line, node.student_id, sizeof(node.student_id));
                (*read_ops)++;
                HeapPush(heap, &heap_size, node);
            }
        }

        // 创建当前批次的输出文件
        snprintf(out_names[out_count], MAX_FILENAME_LEN, "temp_r%d_%03d.csv", round, out_count);
        FILE *fout = fopen(out_names[out_count], "w");
        if (!fout)
            return -1;

        // 归并核心循环：不断取出堆顶（最小值）写入输出，并从对应文件补充新数据
        while (heap_size > 0)
        {
            HeapNode top = HeapPop(heap, &heap_size);
            fputs(top.line, fout); // 直接写入原始 CSV 行
            (*write_ops)++;

            int fi = top.file_idx; // 获取该记录来自哪个文件
            HeapNode node;
            node.file_idx = fi;

            // 从对应文件中读取下一行，如果未到文件末尾，则重新入堆
            if (fgets(node.line, sizeof(node.line), files[fi]))
            {
                ExtractStudentID(node.line, node.student_id, sizeof(node.student_id));
                (*read_ops)++;
                HeapPush(heap, &heap_size, node);
            }
        }

        fclose(fout);

        // 关闭并删除当前批次已合并的旧临时文件，释放磁盘空间
        for (int i = 0; i < batch; i++)
        {
            fclose(files[i]);
            remove(in_names[idx + i]);
        }
        free(files);
        free(heap);

        printf("  -> temp_r%d_%03d.csv (合并了 %d 个文件)\n", round, out_count, batch);
        out_count++;
        idx += batch; // 移动输入文件索引
    }

    printf("  -------------------------------\n");
    printf("  第 %d 轮完成: 读取 %ld, 写入 %ld, 输出文件 %d 个\n",
           round, *read_ops, *write_ops, out_count);
    *out_count_out = out_count;
    return 0;
}

/* ==================== 主入口 ==================== */

/**
 * @brief 外部排序主控制函数
 */
int ExternalSortByStudentID(const char *input_file, const char *output_file)
{
    long total_read = 0, total_write = 0;
    long phase_read, phase_write, split_read, split_write;

    // 使用两个二维数组实现双缓冲，交替存储每一轮的输入和输出文件名
    char names_a[MAX_TEMP_FILES][MAX_FILENAME_LEN];
    char names_b[MAX_TEMP_FILES][MAX_FILENAME_LEN];
    int file_count = 0; // 记录当前轮次的临时文件数量

    printf("################## 外部排序开始 ##################\n");
    printf("  输入文件: %s\n", input_file);
    printf("  输出文件: %s\n", output_file);
    printf("  参数: N=100000, M=%d (内存记录数), k=%d (归并路数)\n", MEMORY_LIMIT, K_WAY);

    /* ---- 阶段一：分割 ---- */
    if (SplitPhase(input_file, names_a, &file_count, &split_read, &split_write) != 0)
        return -1;
    total_read += split_read;
    total_write += split_write;

    if (file_count == 0)
    {
        printf("无数据\n");
        return 0;
    }

    // 【修复记录】保存初始文件数，用于最终报告打印 (原代码中 file_count 会在归并后变为 1)
    int initial_file_count = file_count;

    /* ---- 阶段二：多轮归并 ---- */
    int round = 1;
    // cur 指向当前轮的输入文件名数组，next 指向当前轮的输出文件名数组
    char (*cur)[MAX_FILENAME_LEN] = names_a;
    char (*next)[MAX_FILENAME_LEN] = names_b;

    // 当临时文件数大于 1 时，说明还需要继续归并
    while (file_count > 1)
    {
        int new_count = 0;
        if (MergeRound(cur, file_count, next, &new_count, &phase_read, &phase_write, round) != 0)
            return -1;

        total_read += phase_read;
        total_write += phase_write;
        printf("  累计 I/O: 读取 %ld + 写入 %ld = %ld\n", total_read, total_write, total_read + total_write);

        // 交换 cur 和 next 的指针指向（双缓冲思想，避免字符串数组拷贝，提升效率）
        char (*tmp)[MAX_FILENAME_LEN] = cur;
        cur = next;
        next = tmp;

        file_count = new_count; // 更新为下一轮的输入文件数
        round++;
    }

    /* ---- 最终输出：添加表头并生成最终文件 ---- */
    FILE *fin = fopen(cur[0], "r");
    FILE *fout = fopen(output_file, "w");
    if (!fin || !fout)
        return -1;

    // 写入 CSV 表头
    fprintf(fout, "student_id,name,college,course_id,course_name,"
                  "credit,semester,enroll_date,score,is_elective\n");

    // 将排好序的数据追加到表头之后
    char line[1024];
    while (fgets(line, sizeof(line), fin))
        fputs(line, fout);

    fclose(fin);
    fclose(fout);
    remove(cur[0]); // 删除最后一个临时文件

    /* ---- 汇总报告 ---- */
    int merge_rounds = round - 1;
    printf("\n################## 排序完成 ##################\n");
    printf("  ┌─────────────┬────────┬────────┬─────────┐\n");
    printf("  │ 阶段        │ 读取   │ 写入   │ 文件数  │\n");
    printf("  ├─────────────┼────────┼────────┼─────────┤\n");
    // 注：此处使用了 initial_file_count 以显示正确的初始分割文件数
    printf("  │ 分割        │ %6ld │ %6ld │  ->%4d │\n",
           split_read, split_write, initial_file_count);
    printf("  │ 归并(共%d轮) │ %6ld │ %6ld │  ->   1 │\n",
           merge_rounds, total_read - split_read, total_write - split_write);
    printf("  ├─────────────┼────────┼────────┼─────────┤\n");
    printf("  │ 合计        │ %6ld │ %6ld │         │\n", total_read, total_write);
    printf("  └─────────────┴────────┴────────┴─────────┘\n");
    printf("  总 I/O 次数: %ld\n", total_read + total_write);
    printf("  输出文件: %s\n", output_file);
    return 0;
}