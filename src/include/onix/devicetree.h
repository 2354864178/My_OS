#ifndef ONIX_DEVICETREE_H
#define ONIX_DEVICETREE_H

#include <onix/types.h>

// 返回内置 DTB 地址和大小（若 size 非空则填充长度）
void *dtb_get_blob(size_t *size);

// 从 DTB 按路径与属性名获取属性值指针与长度（大端原始字节）
// path 形如 "/"、"/chosen"、"/vga_console" 或 "/*/child"（不支持通配符）
// 成功返回 0，失败返回 -1
int dtb_get_prop(const char *path, const char *prop, void **val, u32 *len);

// 读取大端 32 位整型（从指针指向的内存）
u32 dt_be32_read(const void *p);

// 在多个路径中依次查找属性，命中返回 0
int dtb_get_prop_any(const char *paths[], size_t pathnr, const char *prop, void **val, u32 *len);

// 返回指定节点的启用状态：
bool dtb_node_enabled(const char *path);

#endif
