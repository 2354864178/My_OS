#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/types.h>
#include <onix/stat.h>
#include <onix/fs.h>

// 判断文件名是否匹配，并返回剩余部分
static bool match_name(const char *name, const char *entry_name, char **next){
    char *lhs = (char *)name;           // 将输入的路径组件转换为可修改的指针
    char *rhs = (char *)entry_name;     // 将目录项的名称转换为可修改的指针

    // 逐个字符进行判断，直到字符不相等或字符结束
    while (*lhs == *rhs && *lhs != EOS && *rhs != EOS) {
        lhs++;
        rhs++;
    }
    if(*rhs) return false;                          // 如果entry_name还有剩余字符，说明不匹配，直接返回false
    if(*lhs && !IS_SEPARATOR(*lhs)) return false;   // 如果 name 还有字符但不是路径分隔符，说明不匹配
    if (IS_SEPARATOR(*lhs)) lhs++;                  // 如果 name 还有字符且是路径分隔符，跳过分隔符
    *next = lhs;                                    // 返回剩余部分
    return true;
}

// 从目录i节点中查找指定名称的目录项，返回包含目录项的缓冲区指针，并通过参数返回目录项指针和剩余路径
static buffer_t *find_entry(inode_t **dir, const char *name, char **next, dentry_t **result){
    assert(ISDIR((*dir)->desc->i_mode));            // 确保 dir 是一个目录

    u32 entries = (*dir)->desc->i_size / sizeof(dentry_t);   // dir 目录最多子目录数量

    idx_t i = 0;                // 当前处理的目录项索引
    idx_t block = 0;            // 当前数据块号
    buffer_t *buf = NULL;       // 当前数据块的缓冲区
    dentry_t *entry = NULL;     // 当前目录项指针
    idx_t nr = EOF;

    for (; i < entries; i++){
        // 如果当前目录项指针为NULL或超出当前数据块范围，说明需要读取新的数据块
        if(!buf || (u32)entry >= (u32)buf->data + BLOCK_SIZE){
            brelse(buf);                                        // 释放之前的数据块缓冲区
            block = bmap((*dir), i / BLOCK_DENTRIES, false);    // 计算需要读取的块号：目录项索引 / 每块目录项数
            buf = bread((*dir)->dev, block);                    // 读取数据块到缓冲区
            entry = (dentry_t *)buf->data;                      // 将目录项指针指向新数据块的起始位置
        }

        // 判断当前目录项是否匹配输入的名称，如果匹配则返回目录项和剩余路径
        if(match_name(name, entry->name, next)){
            *result = entry;    // 返回找到的目录项
            return buf;         // 返回包含目录项的缓冲区（引用计数+1）
        }
        entry = (dentry_t *)((u32)entry + sizeof(dentry_t));  // 移动到下一个目录项
    }
    brelse(buf);    // 释放最后一个数据块缓冲区
    return NULL;    // 返回NULL说明没找到目录
}

// 在 dir 目录中添加 name 目录项
static buffer_t *add_entry(inode_t *dir, const char *name, dentry_t **result){
    char *next = NULL;
    buffer_t *buf = find_entry(&dir, name, &next, result);  
    if(buf) return buf;    // 如果目录项已存在，直接返回包含目录项的缓冲区

    // name 中不能有路径分隔符
    for (size_t i = 0; i < NAME_MAX && name[i]; i++){
        assert(!IS_SEPARATOR(name[i]));
    }

    idx_t i = 0;           // 目录项索引
    idx_t block = 0;       // 数据块号
    dentry_t *entry;       // 目录项指针

    // 循环查找空闲目录项，直到找到或需要分配新块
    for (; true; i++, entry++){
        if (!buf || (u32)entry >= (u32)buf->data + BLOCK_SIZE){
            brelse(buf);
            block = bmap(dir, i / BLOCK_DENTRIES, true);    // true表示需要分配新块
            buf = bread(dir->dev, block);                   // 读取块
            entry = (dentry_t *)buf->data;  
        }
        if (i * sizeof(dentry_t) >= dir->desc->i_size){
            entry->inode = 0;                   // 标记为空闲目录项
            dir->desc->i_size = (i + 1) * sizeof(dentry_t);  // 扩展目录大小
            dir->buffer->dirty = true;          // 标记目录inode为脏
        }
        if (entry->inode) continue;             // 如果目录项已被占用，继续查找下一个目录项
        strncpy(entry->name, name, NAME_MAX);   // 找到空闲位置，创建新目录项。复制名称
        buf->dirty = true;                      // 标记目录项所在块为脏，表示需要写回设备   
        dir->desc->i_time = time();             // 更新目录的最后修改时间
        dir->buffer->dirty = true;              // 标记目录inode为脏，表示需要写回设备
        *result = entry;                        // 返回新创建的目录项
        return buf;
    };
}

#include <onix/task.h>
void dir_test(){
    task_t *task = running_task();
    inode_t *root = task->root;     // 获取当前任务的根目录i节点
    root->count++;                  // 给根目录i节点加引用，防止被回收
    char *next = NULL;
    dentry_t *entry = NULL;
    buffer_t *buf = NULL;

    char pathname[] = "d1/d2/d3/d4";
    char *name = pathname;  // 从路径的起始位置开始解析路径组件
    buf = find_entry(&root, name, &next, &entry);  // 查找路径的第一个组件
    brelse(buf);            // 释放查找过程中使用的缓冲区

    dev_t dev = root->dev;      // 获取根目录所在设备号
    iput(root);                 // 释放根目录i节点的引用
    root = iget(dev, entry->inode);  // 获取找到的目录项对应的i节点

    name = next;    // 更新路径指针，指向下一个组件
    buf = find_entry(&root, name, &next, &entry);  // 查找路径的第二个组件
    brelse(buf);        // 释放查找过程中使用的缓冲区

    iput(root);     // 释放上一个目录i节点的引用
    root = iget(dev, entry->inode);  // 获取找到的目录项对应的i节点

    name = next;    // 更新路径指针，指向下一个组件
    buf = find_entry(&root, name, &next, &entry);  // 查找路径的第三个组件
    brelse(buf);    // 释放查找过程中使用的缓冲区

    iput(root);     // 释放上一个目录i节点的引用
    root = iget(dev, entry->inode);  // 获取找到的目录项对应的i节点

    name = next;    // 更新路径指针，指向下一个组件
    buf = find_entry(&root, name, &next, &entry);  // 查找路径的第四个组件
    brelse(buf);    // 释放查找过程中使用的缓冲区

    iput(root);     // 释放上一个目录i节点的引用
    root = iget(dev, entry->inode);  // 获取找到的目录项对应的i节点
}
