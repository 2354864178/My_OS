#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/types.h>
#include <onix/stat.h>
#include <onix/task.h>
#include <onix/fs.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)  // 内核日志宏

#define P_EXEC IXOTH    // 其他用户执行权限位
#define P_WRITE IOWOTH  // 其他用户写权限位
#define P_READ IROTH    // 其他用户读权限位

// 判断当前任务是否有权限访问指定的i节点
static bool permission(inode_t *inode, u16 umask){
    u16 mode = inode->desc->i_mode;             // 获取i节点的权限模式
    
    if(!inode->desc->i_nlinks) return false;    // 如果链接数为0，说明文件已被删除，拒绝访问

    task_t *task = running_task();              // 获取当前运行的任务
    if(task->uid == KERNEL_USER) return true;   // 如果是内核用户，直接允许访问
    if(task->uid == inode->desc->i_uid) mode >>= 6;         // 用户权限位在高3位
    else if(task->gid == inode->desc->i_gid) mode >>= 3;    // 所属组权限位在中间3位

    if((mode & umask & 0b111) == umask) return true;   // 如果权限位满足要求，允许访问
    return false;   // 否则拒绝访问
}

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

// 从目录i节点中查找指定名称的目录项
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

// 获取pathname路径对应的父目录i节点
inode_t *named(char *pathname, char **next){
    inode_t *dir = NULL;            // 当前目录i节点
    task_t *task = running_task();  // 获取当前运行的任务
    char *left = pathname;          // 路径剩余部分指针

    if(IS_SEPARATOR(left[0])){
        dir = task->root;       // 如果路径以分隔符开头，说明从根目录开始解析
        left++;                 // 路径剩余部分跳过分隔符
    }
    else if(left[0]) dir = task->cwd;  // 否则从当前工作目录开始解析
    
    dir->count++;               // 给起始目录i节点加引用，防止被回收
    *next = left;               // 返回路径剩余部分指针

    if(!*left) return dir;      // 如果路径没有剩余部分，直接返回起始目录i节点
    char *right = strrsep(left);   // 寻找路径剩余部分中最后一个分隔符的位置，分割出最后一个组件
    if(!right || right < left) return dir;  // 如果没有找到分隔符，说明路径只有一个组件，直接返回起始目录i节点
    right++;
    *next = left;               // 更新路径剩余部分指针，指向下一个组件的起始位置
    dentry_t *entry = NULL;           // 目录项指针
    buffer_t *buf = NULL;              // 缓冲区指针
    while(true){
        buf = find_entry(&dir, left, next, &entry);  // 在当前目录i节点中查找路径组件对应的目录项
        if(!buf) goto failure;    // 如果没有找到目录项，说明路径无效，跳转到失败处理

        dev_t dev = dir->dev;      // 获取当前目录所在设备号
        iput(dir);                 // 释放当前目录i节点的引用
        dir = iget(dev, entry->inode);  // 获取找到的目录项对应的i节点，继续解析下一个组件
        if(!ISDIR(dir->desc->i_mode) || !permission(dir, P_EXEC)) goto failure;       // 如果获取i节点失败，说明路径无效，跳转到失败处理
        if(right == *next) goto success;  // 如果路径剩余部分没有更多组件，说明已经解析到目标目录，跳转到成功处理
        left = *next;   // 更新路径剩余部分指针，指向下一个组件的起始位置
    }
success:
    brelse(buf);    // 释放最后一个数据块缓冲区
    return dir;     // 返回目标目录i节点

failure:
    brelse(buf);    // 释放最后一个数据块缓冲区
    iput(dir);      // 释放当前目录i节点的引用
    return NULL;    // 返回NULL表示路径无效
}

// 获取 pathname 对应的 inode
inode_t *namei(char *pathname){
    char *next = NULL;
    inode_t *dir = named(pathname, &next);  // 找到父目录的inode
    if(!dir) return NULL;
    if(!(*next)) return dir; // 如果路径没有剩余部分，说明 pathname 就是父目录，直接返回父目录的 inode

    char *name = next;
    dentry_t *entry = NULL;
    buffer_t *buf = find_entry(&dir, name, &next, &entry);
    if (!buf){
        iput(dir);
        return NULL;
    }
    inode_t *inode = iget(dir->dev, entry->inode);
    iput(dir);
    brelse(buf);
    return inode;
}

void dir_test(){
    char pathname[] = "/";
    char *next = NULL;
    inode_t *dir = named(pathname, &next);
    iput(dir);

    dir = namei("/home/hello.txt");
    LOGK("inode num: %d\n", dir->num);
    iput(dir);
}
