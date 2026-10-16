# 恶魔城：白夜协奏曲 原生静态重编译版 (Castlevania: Harmony of Dissonance Recomp)

本项目是 Game Boy Advance (GBA) 经典动作角色扮演游戏 **《恶魔城：白夜协奏曲》（Castlevania: Harmony of Dissonance, USA 版）** 的 Windows x64 原生静态重编译版本。基于 [GBARecomp](https://github.com/mstan/gbarecomp) 框架并集成了成熟的原生 GUI 启动器 [recomp-ui](https://github.com/mstan/recomp-ui)。

---

## 架构说明与运行原理

本项目采用 **静态重编译 CPU、模拟外设硬件** 的架构设计：
1. **CPU 转译**：原卡带中 ARM7TDMI（ARM / Thumb 模式）机器码在编译期离线转译为原生 C++ 源码（位于 `src/game/`），由 MinGW-w64 编译为原生机器码执行。
2. **纯净玩家模式与解释器回退**：运行时默认关闭动态编译器依赖（`GBARECOMP_SELFHEAL_RECOMPILE=0`），如遇未静态覆盖的指令或跳表，将由内置 Stage-1 解释器安全承接，无需用户安装任何编译环境。
3. **硬件级外设模拟**：PPU 图像渲染（Mode 0-5、OBJ、图层窗口与半透明混色）、DMA 传输、定时器、中断管理、Flash512 存档芯片均由底层运行时模拟。

> [!NOTE]
> **验证范围说明**：当前已完成 BIOS 启动动画、Konami 商标、标题画面、新游戏开场动画、主角移动、基础战斗与主菜单的 `FULLY_STATIC` 零丢失验证。尚未进行全部路线、多结局及 Boss Rush 的全流程通关长测，不得视为全游戏所有未知分支均已 100% 静态化。

---

## 游戏文件要求与严格校验

本项目为纯净引擎与重编译器，**严禁且不包含** 任何受版权保护的商业 ROM 数据或 GBA BIOS 固件。玩家必须自行提供合法提取的文件：

| 属性 | 要求值 |
|---|---|
| **游戏版本** | Castlevania: Harmony of Dissonance (USA) |
| **Game Code** | `ACHE` |
| **ROM 大小** | `8,388,608` 字节 (8 MiB) |
| **ROM SHA-1** | `b90da0d9be0b3a0893cd9e2c399056bcf9579e21` |
| **ROM CRC32** | `88C1B562` |
| **BIOS 大小** | `16,384` 字节 (16 KiB) |
| **BIOS SHA-1** | `300c20df6731a33952ded8c436f7f186d25d3492` |
| **存档类型** | Flash512 (64 KiB, `CASTLEVANIA1.00` 签名) |

启动器与底层均配置严格 SHA-1 与 Game Code 门禁，非匹配版本或修改版 ROM 将被明确拦截。

---

## recomp-ui 原生启动器使用指南

双击运行 `CastlevaniaHarmonyOfDissonanceRecomp.exe`（或 `hod.exe`）即可进入原生图形启动器：

### 1. 游戏页面 (Game)
- **Select ROM / Select BIOS**：浏览并选取您的 ROM 与 BIOS 文件。
- **实时哈希与大小校验**：展示文件 SHA-1 校验结果及有效性提示。
- **Play**：启动游戏。
- **Skip Launcher**：勾选后下次双击直接进入游戏。
- **Reset Settings**：重置所有设置项为出厂默认值。

### 2. 输入配置 (Input)
- 完整支持键盘与 SDL2 GameController 手柄。
- 支持手柄热插拔与自定义按键绑定。
- **默认操作习惯**：
  - **移动**：方向键 / WASD
  - **跳跃**：`Z` (GBA A 键)
  - **攻击与确认**：`X` (GBA B 键)
  - **后撤步**：`Q` / `A` (GBA L 键)
  - **前冲 / 副武器与法术**：`E` / `S` (GBA R 键)
  - **暂停菜单**：`Enter` (GBA Start 键)
  - **地图查看**：`Right Shift` / `Backspace` (GBA Select 键)
  - **全屏切换**：`Alt + Enter`

### 3. 画面设置 (Video)
- 支持 1x ~ 6x 整数倍平滑缩放。
- 原生 240×160 分辨率与 3:2 纵横比锁定。
- 窗口模式与无缝全屏切换。

### 4. 音频设置 (Audio)
- 主音量调节滑块，设置实时生效并持久化保存。

### 5. 命令行启动与重新打开启动器
- **重新打开启动器**：传入 `--launcher` 参数即可强制呼出启动器界面。
- **免界面直接启动**：支持 `--rom <path>`、`--bios <path>`、`--save <path>`、`--config <path>` 参数。

---

## 存档兼容性说明

- 本作使用标准的 **Flash 512 Kbit (64 KiB = 65,536 字节)** 存档芯片（签名：`CASTLEVANIA1.00`）。
- **非 SRAM 游戏**：请勿使用 32 KiB 的 SRAM 存档替换。
- 默认新存档将写入 `saves/hod.sav`。
- 启动器自动向前兼容程序同级目录的既有历史存档（如 `Castlevania - Harmony of Dissonance(US)(Konami)(64Mb).sav`、`hod.sav`），绝不会意外覆盖。
- 命令行 `--save` 拥有最高优先级。

---

## 开发者增量构建

若需在本地重新构建宿主：
```powershell
# 1. 依赖审计与环境设置（可选，若已存在 _sdl2 则自动识别）
.\scripts\setup_sdl2.ps1

# 2. 一键本地增量构建（默认并行度 Jobs = 2）
.\scripts\build.ps1 -Jobs 2

# 3. 运行启动器自动化契约测试
.\scripts\build.ps1 -BuildContractTest
```

---

## 许可证与法律声明

- 《恶魔城：白夜协奏曲》（Castlevania: Harmony of Dissonance）游戏版权与商标归 Konami Digital Entertainment Co., Ltd. 所有。
- 本项目重编译框架与启动器遵循 PolyForm Noncommercial 1.0.0 与 MIT 许可证。详见 `LICENSE` 与 `THIRD_PARTY_NOTICES.md`。
