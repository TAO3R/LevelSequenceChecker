# Level Sequence Checker

Unreal Engine 编辑器插件：Level Sequence 资产保存时自动检查工具。

在保存 Level Sequence 资产时自动运行一组检查规则，发现的问题输出到 Message Log（窗口 → 开发者工具 → 消息日志 → "LevelSequence 检查器" 分页），帮助在资产入库前拦截常见错误。

## 检查规则

| RuleId | 规则               | 说明                                                |
| ------ | ---------------- | ------------------------------------------------- |
| S-01   | 空 Track          | Track 不包含任何 Section 或关键帧；含 Camera Cuts Track 专项校验 |
| S-02   | 空 Section        | Section 不包含任何关键帧                                  |
| S-03   | 重复轨道             | 同一绑定对象上存在多条相同类型的轨道                                |
| S-04   | Track 数量超限       | Track 数量超过阈值                                      |
| S-05   | Section 数量超限     | Section 数量超过阈值                                    |
| K-01   | 关键帧值 NaN/Inf     | 数值通道（Float/Double）中存在 NaN 或 Inf 关键帧               |
| B-01   | Spawnable 碰撞未关闭  | Spawnable 的碰撞未按预期关闭                               |
| B-02   | Spawnable 物理模拟开启 | Spawnable 开启了物理模拟                                 |
| B-03   | Possessable 绑定丢失 | Possessable 绑定在关卡中找不到目标对象                         |
| R-01   | 丢失资源引用           | 引用的资源包缺失（带跨会话缓存，减少误报）                             |

各规则的开关与参数可在 **项目设置 → 插件 → Level Sequence 资产检查工具** 中配置。

## 安装

1. 将本仓库克隆到项目的 `Plugins/` 目录下：
   
   ```powershell
   cd <项目根目录>\Plugins
   git clone https://github.com/TAO3R/LevelSequenceChecker.git
   ```

2. 右键 `.uproject` → **Generate Visual Studio project files**，重新生成项目文件。

3. 用 Visual Studio / Rider 编译项目（Development Editor 配置）。

4. 启动编辑器，在 **编辑 → 插件** 中确认 Level Sequence Checker 已启用，然后重启编辑器。

## 用法

启用后，每次保存 Level Sequence 资产会自动触发检查，无需手动操作。

### 控制台命令（编辑器内 `~` 控制台）

| 命令                 | 作用                                                                         |
| ------------------ | -------------------------------------------------------------------------- |
| `LSC.InjectNaNInf` | 测试用：向所有已加载 LevelSequence 的每个数值通道的前 2 个关键帧注入 NaN 和 Inf，随后保存资产即可触发 K-01 规则告警 |
| `LSC.CleanNaNInf`  | 清理用：将所有已加载 LevelSequence 中的 NaN/Inf 关键帧重置为 0，保存后生效                         |

### 控制台变量

| CVar                                     | 作用                                    |
| ---------------------------------------- | ------------------------------------- |
| `LevelSequenceChecker.EnableCheckOnSave` | 开关保存时自动检查（1=启用，0=禁用），同设置面板中的"启用保存时检查" |

## 环境要求

- **引擎版本**：Unreal Engine 5.7（已在 5.7.4 源码版验证；其他版本未测试）
- **操作系统**：Windows 10/11 x64

### 编译工具链

- Visual Studio 2022（Community 及以上），MSVC v143 工具集 **14.44.35207（cl.exe 19.44.35228）**
- Windows SDK **10.0.26100.0**

仓库仅含源码，使用方需具备上述源码编译环境。

## 目录结构

```
LevelSequenceChecker/
├── LevelSequenceChecker.uplugin
└── Source/LevelSequenceChecker/
    ├── Public/    # 模块接口、设置、规则基类、检查结果结构
    └── Private/   # 保存拦截、检查报告、缺失引用缓存、各规则实现
```
