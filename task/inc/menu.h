// menu.h
#ifndef MENU_H
#define MENU_H
#include <stdbool.h>
/* ==========================================
 * 导航状态码 (Navigation Status Codes)
 * 用于控制菜单的跳转、返回和退出逻辑
 * ========================================== */
#define RET_OK 1
#define RET_BACK -1
#define RET_QUIT -2
#define RET_HOME -3

/* ==========================================
 * 预览类型 (Preview Types)
 * 用于 Tab 键触发预览时的数据类型标识
 * ========================================== */
#define PREVIEW_NONE 0    // 无预览功能
#define PREVIEW_STUDENT 1 // 触发学生信息预览
#define PREVIEW_COURSE 2  // 触发课程信息预览
/* ==========================================
 * 核心菜单逻辑
 * ========================================== */

/**
 * @brief 主菜单循环函数
 * 负责渲染菜单界面、处理用户输入，维持程序的主运行逻辑
 */
void Menu_MainLoop(void);
/**
 * @brief 获取当前系统日期字符串
 * @return 指向当前日期字符串的常量指针 (例如 "2026-06-30")
 */
const char *Menu_GetCurrentDate(void);

/* ==========================================
 * 全局 UI 交互接口 (Global UI Interaction Interfaces)
 * 提供终端界面的基础交互与输入功能
 * ========================================== */

/**
 * @brief 启用 ANSI 转义序列
 * 用于在终端(尤其是 Windows CMD/PowerShell)中开启颜色输出和光标控制支持
 */
void EnableANSI(void);
/**
 * @brief 暂停程序并等待用户按下回车键
 * 常用于操作完成后提示“按回车键继续...”
 */
void WaitEnter(void);
/**
 * @brief 带默认值的字符串读取
 * @param prompt 提示语 (例如: "请输入姓名 [默认:张三]: ")
 * @param buffer 存储用户输入的字符数组
 * @param size   字符数组的最大容量，防止缓冲区溢出
 * @return 读取状态或实际读取的字符数
 */
int ReadStringWithDefault(const char *prompt, char *buffer, int size);
/**
 * @brief 支持 Tab 键触发预览的字符串读取
 * @param prompt       提示语
 * @param buffer       存储用户输入的字符数组
 * @param size         字符数组的最大容量
 * @param preview_type 预览类型 (传入 PREVIEW_xxx 宏定义)
 * @return 读取状态或实际读取的字符数
 */
int ReadStringWithTabPreview(const char *prompt, char *buffer, int size, int preview_type);
extern bool g_data_modified; // 数据脏标记：用于退出时提示是否保存
#endif