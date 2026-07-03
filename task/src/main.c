#include "menu.h"           // 菜单模块头文件
#include "data_manager.h"   // 数据管理模块头文件
#include "course_catalog.h" // 课程目录模块头文件
#include "context.h"        // 上下文模块头文件（全局时间等）
#include "data_generator.h" // 数据生成器模块头文件
#include <stdio.h>          // 标准输入输出库
#include <stdlib.h>         // 标准库（包含rand()等函数）
#include <string.h>         // 字符串处理库
#include <dirent.h>         // 目录扫描库 (跨平台支持 Windows MinGW / Linux)
// ================= ★ 终极跨平台高精度计时器 (微秒/纳秒级) ★ =================
// 解决粗粒度计时器误差大的问题，真实反映底层引擎的性能差异
#ifdef _WIN32
// 手动声明 Windows QPC API，避免引入庞大的 <windows.h> 导致编译缓慢或冲突
__declspec(dllimport) int __stdcall QueryPerformanceCounter(long long *lpPerformanceCount);
__declspec(dllimport) int __stdcall QueryPerformanceFrequency(long long *lpFrequency);

static double GetWallTime(void)
{
    static long long freq = 0;
    if (freq == 0)
        QueryPerformanceFrequency(&freq); // 获取CPU高频时钟频率
    long long counter;
    QueryPerformanceCounter(&counter);
    return (double)counter / (double)freq * 1000.0; // 返回毫秒 (带微秒小数)
}
#else
// POSIX 系统 (Linux / MinGW-w64)
#include <time.h>
static double GetWallTime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);                // 获取系统单调时钟，不受系统时间修改影响
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0; // 返回毫秒 (带微秒小数)
}
#endif
int main()
{
    // 打印系统启动界面
    printf("========================================\n");
    printf("  校园选课记录检索与大数据分析系统\n");
    printf("========================================\n");

    // 初始化各个模块
    Context_Init();       // 初始化全局时间上下文
    DM_Init();            // 初始化数据存储引擎
    DataGenerator_Init(); // 初始化数据生成器

    // 输出随机数校验值，用于验证随机数生成器是否正常工作
    printf("[测试] 本次运行随机数校验值: %d\n", rand() % 10000);

    // ================= 新增：启动时自动扫描并选择 CSV 文件 =================
    DIR *dir;
    struct dirent *ent;
    char csv_files[50][256]; // 最多记录 50 个 CSV 文件
    int csv_count = 0;

    // 打开当前目录进行扫描
    if ((dir = opendir(".")) != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            size_t len = strlen(ent->d_name);
            // 检查文件名是否以 .csv 或 .CSV 结尾
            if (len > 4)
            {
                const char *ext = ent->d_name + len - 4;
                if (ext[0] == '.' &&
                    (ext[1] == 'c' || ext[1] == 'C') &&
                    (ext[2] == 's' || ext[2] == 'S') &&
                    (ext[3] == 'v' || ext[3] == 'V'))
                {
                    strcpy(csv_files[csv_count], ent->d_name);
                    csv_count++;
                    if (csv_count >= 50)
                        break; // 防止数组越界
                }
            }
        }
        closedir(dir);
    }

    // 如果找到了 CSV 文件，让用户选择
    if (csv_count > 0)
    {
        printf("\n[系统] 检测到当前目录下有 %d 个 CSV 数据文件：\n", csv_count);
        for (int i = 0; i < csv_count; i++)
        {
            printf("  [%d] %s\n", i + 1, csv_files[i]);
        }
        printf("  [0] 不加载任何文件，直接启动\n");

        int choice = -1;
        printf("\n请选择要加载的数据文件 (0-%d): ", csv_count);

        // 安全读取用户输入
        if (scanf("%d", &choice) != 1)
        {
            while (getchar() != '\n')
                ; // 清除非法输入
            choice = 0;
        }
        else
        {
            while (getchar() != '\n')
                ; // 清除换行符
        }

        if (choice >= 1 && choice <= csv_count)
        {
            // ================= ★ 防卡死提示与强制刷新 ★ =================
            printf("\n============================================================\n");
            printf("  [⏳ 正在解析并导入 %s ，请稍候...]\n", csv_files[choice - 1]);
            printf("  (若数据量达10万条，系统会自动启用哈希引擎加速，可能需要10-20秒)\n");
            printf("  [!] 系统正在全力处理中，请耐心等待，请勿关闭程序...\n");
            printf("============================================================\n");

            // ★ 核心：强制刷新输出缓冲区，确保提示语瞬间“拍”到屏幕上！
            fflush(stdout);
            // ================================================================

            // ★ 记录开始时间（微秒级高精度墙钟时间，包含磁盘 I/O 等待）
            double start_time = GetWallTime();

            // 调用底层导入函数
            int count = DM_LoadFromFile(csv_files[choice - 1]);

            // ★ 计算真实耗时 (毫秒，保留3位小数)
            double end_time = GetWallTime();
            double time_spent = end_time - start_time;

            // 加载完成后的回显
            if (count >= 0)
            {
                printf("\n[✓ 成功] 导入完成！真实耗时: %.3f ms\n", time_spent);
                printf("------------------------------------------------------------\n");
                printf(" 📊 导入统计与系统状态:\n");
                printf("    - 成功写入系统: %d 条有效记录\n", count);
                printf("    - 当前系统总量: %d 条\n", DM_GetDataSize());
                printf("------------------------------------------------------------\n");
                g_data_modified = false; // 标记数据未修改（与文件一致）
            }
            else
            {
                printf("\n[✗ 失败] 加载失败，请检查文件是否存在或格式是否正确！\n");
            }
        }
        else
        {
            printf("\n[系统] 已跳过数据加载，以空数据启动。\n");
        }
    }
    else
    {
        printf("\n[系统] 未检测到 CSV 数据文件，将以空数据启动。\n");
    }
    // =====================================================================
    system("cls"); // 清除Windows命令行屏幕
    // 初始化完成，进入主菜单循环
    printf("\n[系统] 初始化完成，进入主菜单...\n");
    Menu_MainLoop(); // 主菜单循环，处理用户交互

    // 程序退出前清理资源
    DM_Destroy(); // 销毁数据存储引擎，释放资源
    return 0;     // 正常退出程序
}