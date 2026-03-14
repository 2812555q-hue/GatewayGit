#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

// 建议把这里的 67 提出来变成宏，方便以后改
#define LED_PIN 67 

int LED_Set(unsigned int lineNum, int value)
{
    int ret;
    struct gpiod_chip *chip;
    struct gpiod_line *line;

    /* 1. 获取GPIO控制器 */
    // ⚠️ 注意：一定要确认你的 LED 是在 gpiochip1 上！
    // 如果是全志板子，通常 gpiochip1 对应 GPIO Bank 1 (PB/PC等，具体看厂家映射)
    chip = gpiod_chip_open("/dev/gpiochip1"); 
    if (chip == NULL) {
        perror("gpiod_chip_open error"); // 用 perror 能打印具体错误原因
        return -1;
    }

    /* 2. 获取GPIO引脚 */
    line = gpiod_chip_get_line(chip, lineNum);
    if (line == NULL) {
        perror("gpiod_chip_get_line error");
        gpiod_chip_close(chip); // 失败了记得关芯片
        return -1;
    }

    /* 3. 设置GPIO为输出模式 */
    // 这里把默认值直接设为 value，防止“先变0再变1”的闪烁
    ret = gpiod_line_request_output(line, "led_test", value); 
    if (ret < 0) {
        perror("gpiod_line_request_output error");
        gpiod_chip_close(chip);
        return -1;
    }

    /* 4. 设置电平 (其实 Request 时已经设了，这句加强一下) */
    gpiod_line_set_value(line, value);

    /* 5. 释放资源 (重要！) */
    gpiod_line_release(line); // 先释放引脚
    gpiod_chip_close(chip);   // 再关闭控制器
    
    return 0; // 必须返回 0
}

