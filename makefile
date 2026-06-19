# ------------------------------------------------
# 1. 定义变量 (像是做菜准备食材)
# ------------------------------------------------

# 目标文件名字 (最终生成的程序名)
TARGET_NAME = main

# 编译器 (在板子上跑就是 gcc)
CC = gcc

# 存放源文件的文件夹
SRC_DIR = sources
# 存放头文件的文件夹
INC_DIR = includes
# 存放编译中间产物(.o)和最终程序的文件夹
BUILD_DIR = build_x86

# 编译选项: 
# -g (生成调试信息) 
# -Wall (开启所有警告，很有用) 
# -I$(INC_DIR) (告诉编译器去 includes 文件夹找头文件)
CFLAGS = -g -Wall -I$(INC_DIR)

# 链接库: 
# -lgpiod (告诉连接器，我们要用 gpiod 这个库)
LIBS = -lgpiod

# ------------------------------------------------
# 2. 自动寻找文件 (自动化魔法)
# ------------------------------------------------

# 自动找到 sources 目录下所有的 .c 文件 (例如: sources/uartled.c sources/gpio_lib.c)
SRCS = $(wildcard $(SRC_DIR)/*.c)

# 把上面的 .c 列表换成对应的 .o 列表，并且路径改到 build 目录下
# 结果变成: build_x86/uartled.o build_x86/gpio_lib.o
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# 最终生成的可执行文件的完整路径 (build_x86/uartled)
TARGET = $(BUILD_DIR)/$(TARGET_NAME)

# ------------------------------------------------
# 3. 编译规则 (具体的做菜步骤)
# ------------------------------------------------

# 伪目标: 防止目录下有个叫 all 或 clean 的文件导致冲突
.PHONY: all clean

# 默认目标: 直接输 make 就会执行这里
all: $(TARGET)

# [链接步骤] 怎么生成最终的可执行文件
# $@ 代表目标文件 (uartled)
# $^ 代表所有的依赖文件 (即所有的 .o 文件)
# cp命令确保在执行程序的时候可以在build_x86中找到gateway.conf（非常重要）
$(TARGET): $(OBJS)
	@echo "Linking target: $@"
	$(CC) -o $@ $^ $(LIBS)
	@echo "Build successful! Run with: sudo ./$@"
	@cp -f gateway.conf $(BUILD_DIR)/  

# [编译步骤] 怎么把 .c 变成 .o
# % 是通配符，表示"任何"
# $< 代表第一个依赖文件 (对应的 .c 文件)
# $@ 代表目标文件 (对应的 .o 文件)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)  # 如果 build 文件夹不存在，先创建它
	@echo "Compiling: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# [清理步骤] make clean 时执行
clean:
	@echo "Cleaning up..."
	rm -rf $(BUILD_DIR)