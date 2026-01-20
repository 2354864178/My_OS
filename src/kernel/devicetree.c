#include <onix/devicetree.h>
#include <onix/string.h>
#include <onix/types.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)  // 内核日志宏

// objcopy on ../build/device.dtb generates symbols with path separators flattened into underscores
extern u8 _binary____build_device_dtb_start[];
extern u8 _binary____build_device_dtb_end[];

#define FDT_BEGIN_NODE 1
#define FDT_END_NODE 2
#define FDT_PROP 3
#define FDT_NOP 4
#define FDT_END 9

typedef struct fdt_header{  // 设备树头部结构体
    u32 magic;              // 设备树魔数 0xd00dfeed(固定值)
    u32 totalsize;          // 设备树总大小
    u32 off_dt_struct;      // 设备树结构体偏移
    u32 off_dt_strings;     // 字符串偏移
    u32 off_mem_rsvmap;     // 内存保留映射偏移
    u32 version;            // 版本
    u32 last_comp_version;  // 最后兼容版本
    u32 boot_cpuid_phys;    // 启动CPU物理ID
    u32 size_dt_strings;    // 字符串大小
    u32 size_dt_struct;     // 结构体大小
} fdt_header;

static u32 be32(u32 v){
    return __builtin_bswap32(v);
}

void *dtb_get_blob(size_t *size){
    if(size)
        *size = _binary____build_device_dtb_end - _binary____build_device_dtb_start;
    return _binary____build_device_dtb_start;
}

// 组装当前路径用于匹配
static bool path_equal(const char *target, const char *stack[], int depth){
    char buf[128];
    int pos = 0;
    buf[pos++] = '/';
    bool first = true;
    for (int i = 0; i < depth; i++){
        const char *name = stack[i];
        if (!name || name[0] == '\0') continue; // 跳过根节点空名
        size_t nlen = strlen(name);             // 节点名称长度
        if (!first){
            if (pos + 1 >= (int)sizeof(buf)) return false;
            buf[pos++] = '/';
        }
        if (pos + (int)nlen >= (int)sizeof(buf)) return false;
        memcpy(buf + pos, name, nlen);
        pos += nlen;
        first = false;
    }
    buf[pos] = 0;
    return strcmp(target, buf) == 0;
}

// 从 DTB 按路径与属性名获取属性值指针与长度（大端原始字节）
int dtb_get_prop(const char *path, const char *prop, void **val, u32 *len){
    size_t blob_size = 0;
    u8 *blob = dtb_get_blob(&blob_size);    // 获取内置 DTB 地址和大小
    if (!blob) return -1;       

    fdt_header *h = (fdt_header *)blob;     // DTB 头部结构体指针
    if (be32(h->magic) != 0xd00dfeed) return -1;    

    u8 *structs = blob + be32(h->off_dt_struct);    // 结构体起始地址
    u8 *strings = blob + be32(h->off_dt_strings);   // 字符串起始地址
    u8 *p = structs;                                // 遍历结构体起始地址   

    const char *stack[8];   // 路径栈，支持最多8级节点
    int depth = 0;          // 当前路径深度

    while (true){
        u32 token = be32(*(u32 *)p);    // 读取下一个令牌
        p += 4;                         // 移动指针到下一个位置
        switch (token){                 // 处理令牌类型 
        case FDT_BEGIN_NODE:            // 开始节点
        {
            const char *name = (char *)p;   // 节点名称字符串
            size_t nlen = strlen(name);     // 节点名称长度
            if (depth < (int)(sizeof(stack) / sizeof(stack[0])))    // 防止路径栈溢出
                stack[depth++] = name;      // 将节点名称压入路径栈
            p += nlen + 1;
            while ((uintptr_t)p & 3)        // 对齐到4字节边界
                p++;
            break;
        }
        case FDT_END_NODE:          // 结束节点
            if (depth > 0)          // 防止路径栈下溢
                depth--;
            break;
        case FDT_PROP:              // 属性节点
        {
            u32 plen = be32(*(u32 *)p);    // 属性值长度
            p += 4;
            u32 nameoff = be32(*(u32 *)p); // 属性名字符串偏移
            p += 4;
            const char *pname = (char *)(strings + nameoff);    // 属性名字符串

            bool path_ok = path_equal(path, stack, depth);      // 检查当前路径是否匹配目标路径

            if (path_ok && strcmp(prop, pname) == 0) {   // 路径和属性名匹配
                if (val) *val = p;      // 设置属性值指针
                if (len) *len = plen;   // 设置属性值长度
                return 0;
            }

            p += plen;
            while ((uintptr_t)p & 3)
                p++;
            break;
        }
        case FDT_NOP:
            break;
        case FDT_END:
            return -1;
        default:
            return -1;
        }
    }
}

// 读取大端 32 位整型（从指针指向的内存）
u32 dt_be32_read(const void *p)
{
    return __builtin_bswap32(*(const u32 *)p);
}

// 在多个路径中依次查找属性，命中返回 0
int dtb_get_prop_any(const char *paths[], size_t pathnr, const char *prop, void **val, u32 *len)
{
    for (size_t i = 0; i < pathnr; i++)
    {
        if (dtb_get_prop(paths[i], prop, val, len) == 0)
            return 0;
    }
    return -1;
}

// 判断设备节点是否启用：缺省或 "okay" 为启用；其他值一律视为禁用
bool dtb_node_enabled(const char *path){
    void *val = NULL; u32 len = 0;
    if (dtb_get_prop(path, "status", &val, &len) != 0 || !val || len == 0)
        return true; // 未设置 status 视为启用

    // 复制到本地缓冲并保证以 NUL 结尾，防止长度溢出
    char buf[16];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, val, len);
    buf[len] = '\0';

    if (strcmp(buf, "okay") == 0){
        LOGK("DT node %s: status okay\n", path);
        return true;
    }

    return false; // 其他非常见取值，保守视为禁用
}
