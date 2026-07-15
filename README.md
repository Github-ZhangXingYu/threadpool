# ThreadPool

一个轻量级的 C++17 线程池，支持普通 FIFO 队列和优先级队列两种模式。

## 项目结构

```
threadpool/
├── CMakeLists.txt        # CMake 构建配置
├── CMakePresets.json     # MinGW 预设（cmake --preset default）
├── main.cpp              # 演示入口
├── task_queue.h / .cpp   # 任务队列（普通队列 / 优先队列）
├── thread_pool.h / .cpp  # 线程池（4 个 worker 线程）
├── build/                # CMake 构建目录（中间产物）
└── product/              # 最终可执行文件
```

## 编译 & 运行

> 工具链：MinGW g++（D:/mingw64），无需 cmake、无需改环境变量。

**一键编译：**

```bash
# Git Bash / WSL
bash build.sh

# PowerShell / cmd
build.bat
```

产物输出到 `product/threadpool.exe`，运行：

```bash
./product/threadpool.exe     # Git Bash
product\threadpool.exe       # PowerShell / cmd
```

## 关键 API

| 类 | 方法 | 说明 |
|---|---|---|
| `ThreadPool` | `start()` | 启动 4 个 worker 线程 |
| `ThreadPool` | `stop()` | 排空队列，关闭，join 线程 |
| `ThreadPool` | `submit(fn, priority)` | 提交任务，priority 越高越先执行 |
| `ThreadPool` | `getQueueSize()` | 返回队列中等待的任务数 |

## 设计要点

- **不会忙等**：worker 线程用条件变量阻塞等待（`waitPop`），不空转浪费 CPU。
- **不会死锁**：`stop()` 先 `shutdown()` 唤醒所有阻塞线程，再 join。
- **优雅退出**：stop 时先排空队列中残留的任务再退出。
- **优先队列**：自定义比较器，`int` 越大优先级越高。
