#include <onix/device.h>
#include <onix/string.h>
#include <onix/task.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/arena.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)  // 内核日志宏
static device_t devices[DEVICE_NR];             // 设备数组

// 获取空设备槽
static device_t *get_null_device() {        
    for (size_t i = 0; i < DEVICE_NR; i++) {
        if (devices[i].type == DEV_NULL) {
            return &devices[i];
        }
    }
    panic("No null device available");
}

// 控制设备
int device_ioctl(dev_t dev, int cmd, void *args, int flags){    
    device_t *device = device_get(dev);                                     // 根据设备号获取设备
    if (device->ioctl) return device->ioctl(device->ptr, cmd, args, flags); // 调用设备的ioctl函数
    LOGK("ioctl of device %d not implemented!!!\n", dev);                   // 未实现ioctl函数
    return EOF;
}

// 读设备
int device_read(dev_t dev, void *buf, size_t count, idx_t idx, int flags){ 
    device_t *device = device_get(dev);
    if (device->read) return device->read(device->ptr, buf, count, idx, flags);
    LOGK("read of device %d not implemented!!!\n", dev);
    return EOF;
}

// 写设备
int device_write(dev_t dev, void *buf, size_t count, idx_t idx, int flags){
    device_t *device = device_get(dev);
    if(device->write) return device->write(device->ptr, buf, count, idx, flags);
    LOGK("write of device %d not implemented!!!\n", dev);
    return EOF;
}

// 执行块设备请求
static void do_request(request_t *request){
    switch (request->type)
    {
    case REQ_READ:
        device_read(request->dev, request->buf, request->count, request->idx, request->flags);
        break;
    case REQ_WRITE:
        device_write(request->dev, request->buf, request->count, request->idx, request->flags);
        break;
    default:
        panic("do_request: unsupported request type %d\n", request->type);
        break;
    }
}

// 块设备请求
void device_request(dev_t dev, void *buf, u8 count, idx_t idx, int flags, u32 type){
    device_t *device = device_get(dev); // 获取设备指针
    assert(device->type == DEV_BLOCK);  // 断言设备类型为块设备
    idx_t offset = idx + device_ioctl(dev, DEV_CMD_SECTOR_START, NULL, 0); // 计算实际偏移
    if(device->parent) device = device_get(device->parent); // 获取父设备指针
    request_t *request = (request_t *)kmalloc(sizeof(request_t)); // 分配请求结构体内存

    request->dev = dev;         // 设置设备号
    request->type = type;       // 设置请求类型
    request->idx = offset;      // 设置索引
    request->count = count;     // 设置计数
    request->flags = flags;     // 设置标志
    request->buf = buf;         // 设置数据缓冲区
    request->task = NULL;       // 设置发起请求的任务为空

    bool empty = list_empty(&device->requests_list);    // 检查请求队列是否为空
    list_push(&device->requests_list, &request->node);  // 将请求加入
    // 如果请求队列不为空，说明设备正在处理其他请求，当前任务需要阻塞等待
    if(!empty){
        request->task = running_task();                 // 设置发起请求的任务为当前任务
        task_block(request->task, NULL, TASK_BLOCKED);  // 阻塞当前任务
    }

    do_request(request);            // 执行请求
    list_remove(&request->node);    // 从请求队列中移除请求
    kfree(request);                 // 释放请求结构体内存
    if(!list_empty(&device->requests_list)){
        request_t *next_request = element_entry(request_t, node, device->requests_list.tail.prev);
        assert(next_request -> task -> magic == ONIX_MAGIC);    // 校验任务结构的魔数以检测损坏
        task_unlock(next_request->task);                        // 解锁下一个请求的任务
    }
}

// 安装设备
dev_t device_install(int type, int subtype,
    void *ptr, char *name, dev_t parent,
    void *ioctl, void *read, void *write) {
    device_t *device = get_null_device();   // 获取空设备槽
    device->type = type;                    // 设置设备类型
    device->subtype = subtype;              // 设置设备子类型
    device->ptr = ptr;                      // 设置设备指针
    strncpy(device->name, name, NAMELEN);   // 设置设备名称
    device->parent = parent;                // 设置父设备
    device->ioctl = ioctl;                  // 设置ioctl函数指针
    device->read = read;                    // 设置read函数指针
    device->write = write;                  // 设置write函数指针
    return device->dev;                     // 返回设备号（设备在数组中的索引）
}

// 初始化设备数组
void device_init(){
    for (size_t i = 0; i < DEVICE_NR; i++)    {
        device_t *device = &devices[i];         // 获取设备槽
        strcpy((char *)device->name, "null");   // 设置设备名称为"null"
        device->type = DEV_NULL;                // 设置设备类型为空设备
        device->subtype = DEV_NULL;             // 设置设备子类型为空设备
        device->dev = i;                        // 设置设备号为索引值
        device->parent = 0;                     // 设置父设备为0
        device->ioctl = NULL;                   // 设置ioctl函数指针为NULL
        device->read = NULL;                    // 设置read函数指针为NULL
        device->write = NULL;                   // 设置write函数指针为NULL  

        list_init(&device->requests_list);      // 初始化设备请求队列
    }
}

// 根据子类型查找设备
device_t *device_find(int subtype, idx_t idx){
    idx_t nr = 0;
    for (size_t i = 0; i < DEVICE_NR; i++) {
        device_t *device = &devices[i];
        if (device->subtype != subtype) continue;
        if (nr == idx) return device;
        nr++;
    }
    return NULL;
}

// 根据设备号查找设备
device_t *device_get(dev_t dev){
    assert(dev >= 0 && dev < DEVICE_NR);    // 断言设备号有效
    device_t *device = &devices[dev];       // 
    assert(device->type != DEV_NULL);       // 断言设备已安装
    return device;                          // 返回对应设备指针
}