#include <onix/bitmap.h>
#include <onix/string.h>
#include <onix/onix.h>
#include <onix/assert.h>

// 构造位图
void bitmap_make(bitmap_t *map, char *bits, u32 length, u32 offset){
    map->bits = bits;
    map->length = length;
    map->offset = offset;
}

// 位图初始化，全部置为 0
void bitmap_init(bitmap_t *map, char *bits, u32 length, u32 start){
    memset(bits, 0, length);
    bitmap_make(map, bits, length, start);
}

// 测试某一位是否为 1
bool bitmap_test(bitmap_t *map, idx_t index){
    assert(index >= map->offset);

    idx_t idx = index - map->offset;    // 得到位图的索引
    u32 bytes = idx / 8;    // 位图数组中的字节
    u8 bits = idx % 8;      // 该字节中的那一位

    assert(bytes < map->length);

    return (map->bits[bytes] & (1 << bits));    // 返回那一位是否等于 1
}

// 设置位图某位的值
void bitmap_set(bitmap_t *map, idx_t index, bool value){
    
    assert(value == 0 || value == 1);   // value 必须是二值的
    assert(index >= map->offset);

    idx_t idx = index - map->offset;    // 得到位图的索引
    u32 bytes = idx / 8;    // 位图数组中的字节
    u8 bits = idx % 8;      // 该字节中的那一位

    if (value) map->bits[bytes] |= (1 << bits); // 置为 1
    else  map->bits[bytes] &= ~(1 << bits);     // 置为 0
}

// 从位图中得到连续的 count 位
int bitmap_scan(bitmap_t *map, u32 count){
    int start = EOF;                 // 标记目标开始的位置
    u32 bits_left = map->length * 8; // 剩余的位数
    u32 next_bit = 0;                // 下一个位
    u32 counter = 0;                 // 计数器

    // 从头开始找
    while (bits_left-- > 0)
    {
        if (!bitmap_test(map, map->offset + next_bit))  counter++;  // 如果下一个位没有占用，则计数器加一
        else  counter = 0;    // 否则计数器置为 0，继续寻找

        
        next_bit++;     // 下一位，位置加一

        // 找到数量一致，则设置开始的位置，结束
        if (counter == count) {
            start = next_bit - count;
            break;
        }
    }
    
    if (start == EOF) return EOF;   // 如果没找到，则返回 EOF(END OF FILE)

    // 否则将找到的位，全部置为 1
    bits_left = count;
    next_bit = start;
    while (bits_left--)
    {
        bitmap_set(map, map->offset + next_bit, true);
        next_bit++;
    }

    return start + map->offset; // 然后返回索引
}
