Breakout-1:打砖块游戏

一个基于 C++ 和 Raylib 开发的跨平台打砖块游戏，支持单人闯关、多人联机、道具系统及自定义关卡配置。

![C++](https://img.shields.io/badge/C++-17-blue.svg)
![Raylib](https://img.shields.io/badge/Raylib-4.0+-lightgrey.svg)
![ENet](https://img.shields.io/badge/Network-ENet-green.svg)
![License](https://img.shields.io/badge/License-MIT-blue.svg)

一个现代化的打砖块游戏实现，不仅包含了经典玩法，还扩展了网络对战和丰富的视觉特效。游戏逻辑与配置分离，通过 JSON 文件即可轻松修改关卡和难度。

核心特性

- 多种模式：
  - 单人模式：经典闯关，挑战不同难度的砖块布局。
  - 多人联机：支持 Host（主机）和 Client（客户端）模式，基于 ENet 库实现网络同步。
- 道具系统：
  - 扩大挡板 (Grow)：获取 "G" 道具，挡板变宽，接球更容易。
  - 球体分裂 (Split)：获取 "S" 道具，球体数量增加。
  - 增加生命 (Life)：获取 "H" 道具，补充生命值。
- 高度配置化：
  - 游戏窗口、关卡布局、速度、颜色等均通过 `config.json` 配置，无需重新编译即可调整游戏性。
- 视觉特效：
  - 内置粒子系统，砖块击碎时产生碎片飞溅效果。
- 异步加载：
  - 采用异步加载功能，不影响当前操作。

操作说明

- 单人模式或主机下采用左右方向键控制板的运动，客户端采用鼠标光标的位置控制板的位置。
- 球和板之间存在摩擦，或许利用这个会有帮助？
- 球全部掉落后扣减生命值。
- 通过板和球之间的碰撞反弹，利用各种道具，在生命值归零之前将砖块全部击碎吧。
- 按下空格键或E键，或许会有神奇的事情发生......？

项目文件结构

breakout-1/
├── CMakeLists.txt
├── config.json
├── build/
├── .git/
├── tests/
├── main.cpp
├── game.cpp
├── Ball.cpp
├── Paddle.cpp
├── Brick.cpp
├── game.h
├── Ball.h
├── Paddle.h
├── Brick.h
└── README.md

技术栈

- 图形渲染：[Raylib](https://www.raylib.com/) (轻量级、易于使用的图形库)
- 网络通信：[ENet](http://enet.bespin.org/) (用于多人联机对战)
- 数据解析：nlohmann/json (C++ JSON 库)
- 构建系统：CMake
- 语言：C++17

快速开始

1. 环境依赖
请确保你的开发环境中已安装以下工具：
- C++ 编译器 (支持 C++17)
- CMake 3.10+
- Raylib 开发库
- ENet 开发库
- nlohmann/json 开发库

2. 构建与运行
本项目使用 CMake 进行管理。建议在 `build` 目录中进行编译：

```bash
进入项目目录
cd breakout-1

创建并进入构建目录
mkdir -p build
cd build

生成 Makefile 并编译
cmake ..
make

运行游戏
./BreakoutGame
```

AI使用

- 在程序开发过程中，使用了QWEN作为辅助。

团队合作

- 代码采用：李星辰
- PPT制作：罗易
