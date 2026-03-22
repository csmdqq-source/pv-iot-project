# 阶段一：C语言补基础（3月底 ~ 4月底）

> **目标：** 把C语言从"能写"提升到"嵌入式面试能扛住"
> **核心教材：** 《C语言深度剖析》（张先凤/陈正冲）
> **每日投入：** 上午3h + 晚上2.5h = 5.5h

---

## 0. 开发环境搭建（第一天搞定）

### 方案一：Windows 用 VS Code + GCC（推荐，最简单）

**第1步：安装 MSYS2（自带GCC编译器）**
1. 下载 MSYS2：https://www.msys2.org/
2. 安装后打开 MSYS2 终端，执行：
```bash
pacman -S mingw-w64-x86_64-gcc
```
3. 把 `C:\msys64\mingw64\bin` 加到系统环境变量 PATH

**第2步：安装 VS Code**
1. 下载：https://code.visualstudio.com/
2. 安装插件：C/C++（微软官方）、Code Runner

**第3步：验证**
```bash
gcc --version    # 看到版本号就成功了
```

### 方案二：Linux虚拟机（后面学Linux驱动也要用）

如果你已经有Ubuntu虚拟机（你Docker项目应该有），直接用：
```bash
sudo apt install gcc gdb make -y
```

### 写代码 → 编译 → 运行 的流程

```bash
# 1. 写代码
vi test.c          # 或者用 VS Code 打开

# 2. 编译（-Wall 开启所有警告，-g 加调试信息）
gcc -Wall -g test.c -o test

# 3. 运行
./test

# 4. 调试（遇到段错误时用）
gdb ./test
(gdb) run
(gdb) bt            # 看调用栈，定位崩溃位置
```

**建议：每个知识点建一个文件夹**
```
c-learning/
├── week1-pointer/
│   ├── 01_basic_pointer.c
│   ├── 02_array_pointer.c
│   ├── 03_func_pointer.c
│   └── notes.md
├── week2-memory/
│   ├── 01_malloc_free.c
│   ├── 02_struct_align.c
│   └── notes.md
├── week3-keywords/
├── week4-datastructure/
└── README.md
```

---

## 1. 关于《C语言深度剖析》PDF

这本书的完整名称是《C语言深度解剖：解开程序员面试笔试的秘密》，作者陈正冲。
只有约120页，非常精炼。

**获取方式：**
- GitHub 搜 "CS-Books" 或 "C-C-"，多个开源电子书仓库收录了这本PDF
- 搜索 "C语言深度剖析 PDF" 可以找到，文件很小（不到1MB）
- 微信读书/豆瓣读书也有电子版
- 如果想支持正版，淘宝/京东十几块钱

> 这本书不厚，建议打印出来，在纸上做标注效果更好。

---

## 2. 第1周：指针基础（Day 1 ~ Day 7）

### Day 1（周一）：指针是什么

**上午 9:00-12:00**
| 时间 | 做什么 |
|------|--------|
| 9:00-9:45 | 牛客C语言专项刷15题（手机也行），错题截图 |
| 9:45-11:00 | 读《深度剖析》第3章"指针"前半部分（指针定义、`*` 和 `&`） |
| 11:00-12:00 | B站看小甲鱼"指针（一）"或翁恺指针第1讲 |

**晚上 19:30-22:00**
| 时间 | 做什么 |
|------|--------|
| 19:30-20:30 | 写代码验证 `01_basic_pointer.c`（见下方） |
| 20:30-21:30 | 写代码 `02_pointer_swap.c`（用指针交换两个变量） |
| 21:30-22:00 | 写 notes.md 总结今天学到的3个要点 |

**练习代码 01_basic_pointer.c：**
```c
#include <stdio.h>

int main(void)
{
    int a = 42;
    int *p = &a;

    // 验证1：指针存的是地址
    printf("a 的值: %d\n", a);
    printf("a 的地址: %p\n", (void *)&a);
    printf("p 存的值（地址）: %p\n", (void *)p);
    printf("p 指向的值（解引用）: %d\n", *p);

    // 验证2：通过指针修改原变量
    *p = 100;
    printf("修改后 a = %d\n", a);  // 应该是100

    // 验证3：指针本身的大小
    printf("sizeof(int *) = %zu\n", sizeof(int *));    // 64位系统=8
    printf("sizeof(char *) = %zu\n", sizeof(char *));   // 也是8
    printf("sizeof(double *) = %zu\n", sizeof(double *)); // 还是8

    // 思考：为什么所有指针大小都一样？
    // 因为指针存的是地址，地址长度取决于系统位数，和指向类型无关

    return 0;
}
```

**练习代码 02_pointer_swap.c：**
```c
#include <stdio.h>

// 错误写法：值传递，交换不了
void swap_wrong(int a, int b)
{
    int tmp = a;
    a = b;
    b = tmp;
}

// 正确写法：指针传递
void swap_right(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

int main(void)
{
    int x = 10, y = 20;

    printf("交换前: x=%d, y=%d\n", x, y);
    swap_wrong(x, y);
    printf("swap_wrong后: x=%d, y=%d\n", x, y);  // 没变！

    swap_right(&x, &y);
    printf("swap_right后: x=%d, y=%d\n", x, y);   // 交换了！

    // 面试追问：为什么swap_wrong不行？
    // 答：C语言函数参数是值传递，a和b是x和y的副本，
    //     修改副本不影响原变量。传指针才能修改原变量。

    return 0;
}
```

### Day 2（周二）：指针与数组

**上午**
| 时间 | 做什么 |
|------|--------|
| 9:00-9:45 | 牛客刷题15题 |
| 9:45-11:00 | 读《深度剖析》指针与数组关系部分 |
| 11:00-12:00 | 视频：翁恺讲"指针与数组"（这一节必看） |

**晚上**
```c
// 03_array_pointer.c
#include <stdio.h>

int main(void)
{
    int arr[] = {10, 20, 30, 40, 50};
    int *p = arr;  // 数组名就是首元素地址

    // 验证1：两种访问方式等价
    for (int i = 0; i < 5; i++) {
        printf("arr[%d]=%d,  *(p+%d)=%d,  *(arr+%d)=%d\n",
               i, arr[i], i, *(p + i), i, *(arr + i));
    }

    // 验证2：指针运算（+1 跳过一个 int 的大小）
    printf("\np 的值: %p\n", (void *)p);
    printf("p+1 的值: %p\n", (void *)(p + 1));
    printf("差值: %ld 字节 = sizeof(int)\n",
           (char *)(p + 1) - (char *)p);

    // 验证3：数组名和指针的区别
    printf("\nsizeof(arr) = %zu（整个数组大小）\n", sizeof(arr));
    printf("sizeof(p)   = %zu（指针大小）\n", sizeof(p));
    // 面试高频：数组名不是指针！sizeof 行为不同

    return 0;
}
```

### Day 3（周三）：二级指针 + 指针数组 vs 数组指针

**上午：** 读《深度剖析》对应章节 + 画内存图（拿纸画！）

**晚上：**
```c
// 04_pointer_types.c
#include <stdio.h>

int main(void)
{
    int a = 1, b = 2, c = 3;

    // ============ 指针数组：存放指针的数组 ============
    int *ptr_arr[3] = {&a, &b, &c};  // 3个int*组成的数组
    printf("指针数组:\n");
    for (int i = 0; i < 3; i++) {
        printf("  ptr_arr[%d] 指向的值 = %d\n", i, *ptr_arr[i]);
    }

    // ============ 数组指针：指向数组的指针 ============
    int arr[3] = {10, 20, 30};
    int (*arr_ptr)[3] = &arr;  // 指向 int[3] 这个整体
    printf("\n数组指针:\n");
    printf("  (*arr_ptr)[0] = %d\n", (*arr_ptr)[0]);
    printf("  (*arr_ptr)[1] = %d\n", (*arr_ptr)[1]);

    // ============ 二级指针 ============
    int x = 42;
    int *p = &x;
    int **pp = &p;
    printf("\n二级指针:\n");
    printf("  x = %d\n", x);
    printf("  *p = %d\n", *p);
    printf("  **pp = %d\n", **pp);

    // 记忆口诀：
    // int *p[3]   →  先结合[3]  →  是个数组  →  数组里放指针  →  指针数组
    // int (*p)[3]  →  先结合*   →  是个指针  →  指向数组      →  数组指针

    return 0;
}
```

### Day 4（周四）：函数指针与回调

**上午：** 读《深度剖析》函数指针章节

**晚上：**
```c
// 05_func_pointer.c
#include <stdio.h>

// 两个普通函数
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

// 回调函数：传入一个函数指针
int calculate(int x, int y, int (*operation)(int, int))
{
    return operation(x, y);
}

// 模拟你项目中的 MQTT 回调机制
typedef void (*mqtt_callback_t)(const char *topic, const char *payload);

void on_message(const char *topic, const char *payload)
{
    printf("收到消息 - topic: %s, payload: %s\n", topic, payload);
}

void mqtt_subscribe(const char *topic, mqtt_callback_t cb)
{
    printf("订阅 %s ...\n", topic);
    // 模拟收到消息，调用回调
    cb(topic, "{\"power\": 118.5}");
}

int main(void)
{
    // 函数指针基础
    int (*fp)(int, int) = add;
    printf("通过函数指针调用 add: %d\n", fp(3, 4));

    // 回调函数
    printf("calculate(10, 3, add) = %d\n", calculate(10, 3, add));
    printf("calculate(10, 3, sub) = %d\n", calculate(10, 3, sub));

    // MQTT 回调模拟（联系你的项目！）
    printf("\n模拟MQTT回调:\n");
    mqtt_subscribe("pv/001/data", on_message);

    return 0;
}
```

### Day 5（周五）：malloc/free + 内存泄漏

**上午：** 读《深度剖析》内存管理部分

**晚上：**
```c
// 06_malloc_free.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 错误示范1：忘记free
void memory_leak(void)
{
    int *p = (int *)malloc(100 * sizeof(int));
    if (p == NULL) {
        printf("malloc失败!\n");
        return;
    }
    p[0] = 42;
    printf("leak函数: p[0] = %d\n", p[0]);
    // 没有free(p)! 内存泄漏！
}

// 错误示范2：free后继续使用（悬空指针）
void dangling_pointer(void)
{
    int *p = (int *)malloc(sizeof(int));
    *p = 100;
    free(p);
    // p 现在是悬空指针！
    // *p = 200;  // 未定义行为！可能崩溃

    p = NULL;  // 正确做法：free后置NULL
}

// 正确写法：动态数组
void correct_usage(void)
{
    int n = 5;
    int *arr = (int *)malloc(n * sizeof(int));
    if (arr == NULL) {
        return;
    }

    for (int i = 0; i < n; i++) {
        arr[i] = i * 10;
    }

    for (int i = 0; i < n; i++) {
        printf("arr[%d] = %d\n", i, arr[i]);
    }

    free(arr);
    arr = NULL;  // 好习惯！
}

int main(void)
{
    memory_leak();       // 有泄漏
    dangling_pointer();  // 有悬空指针
    correct_usage();     // 正确

    // 面试要点：
    // 1. malloc 返回值一定要检查是否为 NULL
    // 2. free 之后立即把指针置 NULL
    // 3. 谁 malloc 谁 free，不要跨函数忘记
    // 4. 不要 free 同一块内存两次（double free）

    return 0;
}
```

### Day 6（周六）：struct 内存对齐（笔试必考！）

**上午：** 读《深度剖析》struct章节 + 做牛客错题回顾

**晚上：**
```c
// 07_struct_align.c
#include <stdio.h>

// 对齐规则：
// 1. 成员的偏移量必须是 min(该成员大小, 对齐系数) 的整数倍
// 2. 结构体总大小必须是 min(最大成员大小, 对齐系数) 的整数倍
// 默认对齐系数通常是8（64位系统）

struct A {
    char a;    // 1字节，偏移0
    int b;     // 4字节，偏移4（补3字节padding）
    char c;    // 1字节，偏移8
    // 总计9字节，但要对齐到4的倍数 → 12字节
};

struct B {
    char a;    // 1字节，偏移0
    char c;    // 1字节，偏移1
    int b;     // 4字节，偏移4（补2字节padding）
    // 总计8字节
};

// 相同成员，不同顺序，大小不同！

struct C {
    char a;       // 1字节
    double b;     // 8字节
    int c;        // 4字节
    // 你算算多大？
};

#pragma pack(1)   // 强制1字节对齐
struct D {
    char a;
    int b;
    char c;
    // 1+4+1 = 6字节
};
#pragma pack()    // 恢复默认

int main(void)
{
    printf("struct A: %zu 字节\n", sizeof(struct A));  // 12
    printf("struct B: %zu 字节\n", sizeof(struct B));  // 8
    printf("struct C: %zu 字节\n", sizeof(struct C));  // 24? 自己算
    printf("struct D: %zu 字节\n", sizeof(struct D));  // 6

    // 面试技巧：画格子图！
    // struct A:
    // [a][.][.][.][b][b][b][b][c][.][.][.]  = 12
    //  0  1  2  3  4  5  6  7  8  9  10 11
    //
    // struct B:
    // [a][c][.][.][b][b][b][b]  = 8
    //  0  1  2  3  4  5  6  7

    // 嵌入式中 #pragma pack(1) 常用于通信协议结构体，
    // 确保结构体内存布局和协议包格式完全一致

    return 0;
}
```

### Day 7（周日）：休息 + 轻度回顾

- 翻看本周 notes.md
- 不看代码，在纸上默写 swap_right 和 struct 对齐画格子
- 能默写出来就过关，写不出来标记明天回头看

---

## 3. 第2周：指针进阶 + 字符串（Day 8 ~ Day 14）

| 天 | 上午读什么 | 晚上写什么 |
|----|-----------|-----------|
| Day 8 | 字符指针与字符串（`char *s` vs `char s[]`） | 手写 strlen、strcpy、strcmp（不用库函数） |
| Day 9 | 指针与const的组合 | 写代码验证 `const int *p`、`int *const p`、`const int *const p` |
| Day 10 | 野指针、void指针 | 写5个"故意犯错"的程序，看编译器怎么报错 |
| Day 11 | 指针做函数参数的各种形式 | 写一个函数：用指针参数返回数组的最大值、最小值、平均值 |
| Day 12 | 动态二维数组 | 用 malloc 创建 m×n 矩阵，填充数据，打印，释放 |
| Day 13 | 复杂声明解读（笔试题型） | 练习解读：`int (*(*fp)(int))[10]` 这类声明 |
| Day 14 | 休息 + 回顾 | 默写 strlen、strcpy |

**Day 8 核心代码：**
```c
// 08_string_funcs.c
#include <stdio.h>

// 手写 strlen
int my_strlen(const char *s)
{
    int len = 0;
    while (*s != '\0') {
        len++;
        s++;
    }
    return len;
}

// 手写 strcpy
char *my_strcpy(char *dest, const char *src)
{
    char *ret = dest;
    while ((*dest++ = *src++) != '\0')
        ;  // 经典写法，赋值和判断合一
    return ret;
}

// 手写 strcmp
int my_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int main(void)
{
    char str[] = "hello";
    char buf[20];

    printf("my_strlen(\"%s\") = %d\n", str, my_strlen(str));

    my_strcpy(buf, str);
    printf("my_strcpy → buf = \"%s\"\n", buf);

    printf("my_strcmp(\"abc\", \"abd\") = %d\n", my_strcmp("abc", "abd"));
    printf("my_strcmp(\"abc\", \"abc\") = %d\n", my_strcmp("abc", "abc"));

    return 0;
}
```

---

## 4. 第3周：关键字 + 位操作（Day 15 ~ Day 21）

| 天 | 上午读什么 | 晚上写什么 |
|----|-----------|-----------|
| Day 15 | `static` 三种用法 | 写代码验证：static局部变量、static全局变量、static函数 |
| Day 16 | `volatile` + `const` | 写代码：volatile防优化场景；const指针组合辨析 |
| Day 17 | `extern`、`typedef` vs `#define` | 写多文件编译项目（a.c 用 extern 引用 b.c 的变量） |
| Day 18 | 宏定义陷阱 | 写10个宏定义的坑（如 `#define MUL(x) x*x` 传 `1+2` 会怎样） |
| Day 19 | 位操作（置位、清位、翻转） | 写一套位操作工具函数（见下方代码） |
| Day 20 | 大小端判断 + 联合体union | 写代码判断当前系统大小端 |
| Day 21 | 休息 + 回顾 | 默写位操作宏 |

**Day 19 核心代码：**
```c
// 09_bit_operations.c
#include <stdio.h>

// 嵌入式最常用的4个位操作
#define SET_BIT(reg, bit)     ((reg) |= (1U << (bit)))    // 置位
#define CLR_BIT(reg, bit)     ((reg) &= ~(1U << (bit)))   // 清位
#define TOGGLE_BIT(reg, bit)  ((reg) ^= (1U << (bit)))    // 翻转
#define GET_BIT(reg, bit)     (((reg) >> (bit)) & 1U)     // 读位

// 实际场景：STM32 GPIO 控制就是这些操作
// GPIOA->ODR |= (1 << 5);   // PA5 输出高电平（点亮LED）
// GPIOA->ODR &= ~(1 << 5);  // PA5 输出低电平

int main(void)
{
    unsigned int reg = 0x00;

    SET_BIT(reg, 0);    // bit0 置1
    SET_BIT(reg, 3);    // bit3 置1
    printf("SET bit0,3:  reg = 0x%02X (二进制: 0000 1001)\n", reg);

    CLR_BIT(reg, 0);    // bit0 清0
    printf("CLR bit0:    reg = 0x%02X (二进制: 0000 1000)\n", reg);

    TOGGLE_BIT(reg, 3); // bit3 翻转
    printf("TOGGLE bit3: reg = 0x%02X (二进制: 0000 0000)\n", reg);

    reg = 0xA5;  // 1010 0101
    printf("\nreg = 0x%02X\n", reg);
    for (int i = 7; i >= 0; i--) {
        printf("bit%d = %d\n", i, GET_BIT(reg, i));
    }

    // 面试题：不用临时变量交换两个整数
    int a = 10, b = 20;
    a ^= b;
    b ^= a;
    a ^= b;
    printf("\n异或交换: a=%d, b=%d\n", a, b);

    return 0;
}
```

---

## 5. 第4周：数据结构手写（Day 22 ~ Day 28）

| 天 | 上午读什么 | 晚上写什么 |
|----|-----------|-----------|
| Day 22 | 单链表原理（画图！） | 手写：链表创建、尾插、头插、打印 |
| Day 23 | 链表操作 | 手写：链表删除节点、查找节点 |
| Day 24 | 链表翻转（面试Top1高频题） | **链表翻转写3遍**（迭代法 + 递归法） |
| Day 25 | 栈和队列 | 用数组实现栈；用数组实现队列 |
| Day 26 | 环形缓冲区（嵌入式核心） | **手写Ring Buffer**（见下方代码） |
| Day 27 | 排序算法 | 手写冒泡排序 + 快速排序 |
| Day 28 | **总复习** | 不看任何参考，默写：链表翻转、Ring Buffer、struct对齐计算 |

**Day 26 核心代码（嵌入式必会）：**
```c
// 10_ring_buffer.c
// 环形缓冲区：嵌入式中用于 UART 收发、传感器数据缓存
// 你的项目中 STM32 接收 A7670E 数据就可以用这个

#include <stdio.h>
#include <string.h>

#define RING_BUF_SIZE 8

typedef struct {
    unsigned char buf[RING_BUF_SIZE];
    int head;  // 写指针
    int tail;  // 读指针
} ring_buf_t;

void ring_init(ring_buf_t *rb)
{
    memset(rb->buf, 0, RING_BUF_SIZE);
    rb->head = 0;
    rb->tail = 0;
}

int ring_is_full(ring_buf_t *rb)
{
    return ((rb->head + 1) % RING_BUF_SIZE) == rb->tail;
}

int ring_is_empty(ring_buf_t *rb)
{
    return rb->head == rb->tail;
}

int ring_write(ring_buf_t *rb, unsigned char data)
{
    if (ring_is_full(rb)) {
        return -1;  // 满了
    }
    rb->buf[rb->head] = data;
    rb->head = (rb->head + 1) % RING_BUF_SIZE;
    return 0;
}

int ring_read(ring_buf_t *rb, unsigned char *data)
{
    if (ring_is_empty(rb)) {
        return -1;  // 空的
    }
    *data = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) % RING_BUF_SIZE;
    return 0;
}

int main(void)
{
    ring_buf_t rb;
    ring_init(&rb);

    // 写入5个数据
    for (int i = 0; i < 5; i++) {
        ring_write(&rb, 'A' + i);
        printf("写入: %c\n", 'A' + i);
    }

    // 读出3个
    unsigned char ch;
    for (int i = 0; i < 3; i++) {
        ring_read(&rb, &ch);
        printf("读出: %c\n", ch);
    }

    // 再写入4个（验证环形特性）
    for (int i = 0; i < 4; i++) {
        int ret = ring_write(&rb, '1' + i);
        if (ret == 0) printf("写入: %c\n", '1' + i);
        else printf("缓冲区满!\n");
    }

    // 全部读出
    printf("\n全部读出:\n");
    while (!ring_is_empty(&rb)) {
        ring_read(&rb, &ch);
        printf("  %c\n", ch);
    }

    return 0;
}
```

---

## 6. 阶段一完成自检清单

完成以下全部打勾，才算阶段一通关：

**指针：**
- [ ] 能口述 `const int *p` / `int *const p` / `const int *const p` 含义
- [ ] 能口述指针数组和数组指针的区别
- [ ] 能手写函数指针+回调的代码
- [ ] 能解释为什么所有类型的指针 sizeof 都一样

**内存：**
- [ ] 能画出任意 struct 的内存布局图并算出 sizeof
- [ ] 能说清 malloc/free 的注意事项（检查NULL、free后置NULL、避免double free）
- [ ] 能说清堆和栈的区别

**关键字与位操作：**
- [ ] 能说清 static 的3种用法
- [ ] 能举出 volatile 在嵌入式中的使用场景（中断变量、硬件寄存器）
- [ ] 能手写置位/清位/翻转/读位的宏
- [ ] 能写代码判断大小端

**数据结构：**
- [ ] 不看参考，手写链表翻转（5分钟内）
- [ ] 不看参考，手写环形缓冲区
- [ ] 手写 strlen / strcpy

**刷题：**
- [ ] 牛客C语言专项完成100题+，正确率 > 75%
