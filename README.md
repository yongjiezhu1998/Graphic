# GraphicsDemo（音频链路图 / DSP 拓扑）

一个基于 Qt Widgets 的 `QGraphicsScene` 项目，面向**调音/系统工程师**绘制 **音频信号流图**（非实时 DSP，仅拓扑与参数编辑）。

## 功能概览

- **左侧模块库**：停靠栏中的树形列表（按 I/O、动态、滤波、EQ、分频分组）；**拖放到画布**即可在落点创建对应模块
- **模块类型**：输入、输出、增益、二阶滤波器（LP/HP/BP/Notch）、三段参量 EQ、二/三分频（多输出）
- **端口连线**：从输出端口拖到输入端口；分频器多路输出可分别接功放通道
- 三次贝塞尔连线；移动模块时路径自动刷新；箭头表示流向
- 双击模块主体或 **F4** 打开参数对话框；双击标题行内重命名
- 右键 **添加音频模块**；撤销/重做；**拖拽移动模块**整段操作为一步撤销；**编辑 → Undo stack history…** 用 `QUndoView` 查看当前标签的撤销栈（调试）；**多选 2 个及以上模块**时 **编辑 → Align & distribute** 提供左/右/上/下对齐、水平/垂直居中对齐、横向/纵向平均分布（一步 `MoveItemsCommand`）；Ctrl+拖动复制选中模块
- 鼠标滚轮缩放；框选与选中高亮
- **多文档标签页**：主窗口以 Tab 管理多个独立场景；**新建标签**（Ctrl+T）、**关闭标签**（Ctrl+W）、**在新标签中打开**（Ctrl+Shift+O）；未保存修改的标签名带 `*`；关闭或覆盖前会提示保存
- 文件菜单：**保存 / 另存为 / 打开**（作用于**当前标签**；JSON `version` 2：`kind`、`params`、端口级连接 `fromModule`/`fromPort`/`toModule`/`toPort`；兼容旧版 v1）
- **设备**菜单：将当前场景以 **紧凑 UTF-8 JSON** 下发（与存盘格式一致，末尾换行）
  - **TCP**：连接 `主机:端口` 后发送，适合本机声卡/PC 端服务监听
  - **串口**：选择端口与波特率后发送，适合嵌入式 MCU/DSP（需安装 Qt SerialPort 模块后编译）

**串口发送若提示 Permission denied（Linux）**：串口设备（如 `/dev/ttyUSB0`）通常属 `dialout` 组，当前用户需加入该组并重新登录：`sudo usermod -aG dialout $USER`。

## 环境要求

- CMake 3.16 或更高
- C++17 编译器
- Qt Widgets、**Qt Network**（TCP 下发）
- 可选：**Qt SerialPort**（串口下发；未检测到则菜单仍可用但会提示需重新编译）

## 构建与运行（Windows / PowerShell）

在项目根目录执行：

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\GraphicsDemo.exe
```

如果你使用的是单配置生成器（如 Ninja），运行文件通常位于：

```powershell
.\build\GraphicsDemo.exe
```

## 构建与运行（Linux）

安装依赖（以 Debian / Ubuntu 为例；其他发行版请安装对应的 Qt 开发包）：

```bash
sudo apt update
sudo apt install build-essential cmake
# 任选其一（推荐 Qt 6）
sudo apt install qt6-base-dev qt6-serialport-dev
# 或 Qt 5：sudo apt install qtbase5-dev libqt5serialport5-dev
```

在项目根目录执行：

```bash
cmake -S . -B build
cmake --build build --config Release
./build/GraphicsDemo
```

使用默认 Makefile 或 Ninja 等单配置生成器时，可执行文件一般为 `./build/GraphicsDemo`（不在 `build/Release/` 子目录下）。

## 项目结构

- `AudioTypes.h` / `AudioTypes.cpp`：模块种类与参数字段
- `AudioPropertiesDialog.*`：按模块类型编辑参数
- `ModuleTreeWidget.*`：左侧模块树与拖放 MIME（`AudioDrag.h`）
- `main.cpp`：程序入口（首帧示例链路在 `MainWindow` 内通过 `populateDemoScene` 创建）
- `MainWindow.h/.cpp`：主窗口、多标签、文件/设备菜单、`PatchTransport` 对话框
- `PatchTransport.h/.cpp`：TCP / 串口下发（载荷为 `SceneIo` 与存盘一致的 JSON + 换行）
- `DemoView.h/.cpp`：视图、撤销栈、场景所有权（多标签下每个视图自有场景）
- `ModuleItem.h/.cpp`：模块图元定义（拖拽、选中、锚点计算）
- `ConnectionItem.h/.cpp`：连接线图元定义（路径更新与箭头绘制）
- `Commands.h/.cpp`：撤销/重做命令（新增/删除/复制/样式、**MoveItemsCommand** 拖拽移动等）
- `SceneIo.h/.cpp`：场景 JSON 保存/加载（`format: GraphicsDemoScene`）
- `CMakeLists.txt`：构建配置（Qt5/Qt6 自动适配）

## 撤销/重做（Undo/Redo）实现难点与解决方案

本项目使用 `QUndoStack + QUndoCommand` 来实现撤销/重做，核心难点集中在 **对象所有权与生命周期**。

### 难点 1：删除后无法撤销恢复

- **现象**：删除模块/连线时如果直接 `delete`，对象被销毁，`Ctrl+Z` 无法“复活”它。
- **解决**：删除操作通过命令实现，`redo()` 时只做 `scene()->removeItem(...)`（并从模块的连接列表中解绑），`undo()` 时再 `addItem(...)` 恢复。

### 难点 2：内存泄漏 vs 撤销可恢复的矛盾

- **问题**：为了可撤销，删除后的对象必须暂存；但如果命令长期存在/命令被撤销栈丢弃，必须释放内存，不能泄漏。
- **解决（本项目采用的规则）**：
  - 若对象 **在 `QGraphicsScene` 中**：由 `QGraphicsScene` 统一销毁（程序退出时自动释放）。
  - 若对象 **不在任何 scene（`item->scene()==nullptr`）**：说明处于“删除已生效/撤销状态”，由对应 `QUndoCommand` 析构时 `delete` 释放。

### 难点 3：连接关系与悬空指针

- **问题**：`ModuleItem` 内部维护 `m_connections`，若删除/撤销时不同步更新，会导致悬空指针或重复更新路径。
- **解决**：命令在移除/恢复连接线时同步调用 `ModuleItem::addConnection/removeConnection`，并在恢复后调用 `ConnectionItem::updatePath()` 重新计算路径。

### 难点 4：双击标题无法进入行内编辑

- **现象**：双击模块标题时，模块先被选中/进入拖拽判定，标题文本项没有稳定收到双击事件，导致无法切换到行内编辑状态；而通过 `F2` 弹窗重命名是正常的。
- **原因**：父图元（模块本体）开启了 `ItemIsSelectable/ItemIsMovable`，鼠标事件在父/子图元之间的分发与接受顺序会影响谁能拿到 `mouseDoubleClickEvent`。
- **解决**：在 `ModuleItem` 内部重写 `mouseDoubleClickEvent()`，对双击位置做“是否命中标题区域”的判断；命中则直接调用 `beginInlineRename()` 进入编辑，不再依赖标题子项先接收到事件。同时将标题项 `zValue` 适当提高以增强命中稳定性。

### 额外注意：`QGraphicsScene` 的释放

- **问题**：若 `QGraphicsScene` 用 `new` 创建且长期无 owner，会泄漏。
- **解决（单窗口 / 早期示例）**：可由栈上对象或单一所有者管理生命周期。
- **解决（多标签）**：由 `DemoView` 在构造时标记拥有场景（`ownsScene`），析构时 `setScene(nullptr)` 后 `delete` 场景；**必须先清空 `QUndoStack`**（见下文「多文档 Tab」），再删场景。

### 难点 5：拖拽移动与撤销粒度（一次拖拽 = 一步撤销）

- **问题**：`QGraphicsItem` 默认拖动时，`ItemPositionChange` 会随鼠标连续触发；若每一步都 `push` 撤销命令，撤销会变成「逐像素」级别，无法使用。
- **解决**：
  - 在 **`ItemPositionChange`** 且左键按下时（`QApplication::mouseButtons()`），**仅第一次**记录当前**所有选中 `ModuleItem`** 的 `scenePos()` 作为起点；
  - 在 **`DemoView::mouseReleaseEvent`** 左键释放时调用 `ModuleItem::commitPendingMove(undoStack, scene)`，与起点比较后若有变化则 **`push` 一条 `MoveItemsCommand`**（`Commands` 中实现，`redo/undo` 用 `moveBy` 对齐目标 `scenePos`）。
- **结果**：一次鼠标拖放只产生 **一条**「Move modules」记录；多选拖动时起点在首次位移时一次性采齐。

### 撤销栈历史（调试）

- **编辑** 菜单：**Undo stack history…**，打开对话框中的 **`QUndoView`**，并 `setGroup(m_undoGroup)`，可列出当前活动栈上的撤销/重做步骤（随标签切换而切换活动栈）。
- 依赖 **`QUndoGroup`** 与多标签架构一致；仅用于查看与调试，不改变栈内容。

### 难点 6：多选对齐与平均分布

- **前提**：仅对**当前选中**的 `ModuleItem` 生效，至少 **2** 个模块时菜单 **Align & distribute** 才可用（`updateActionStates` 中按选中数量启用）。
- **几何基准**：一律用 **`sceneBoundingRect()`**（场景坐标）算边、中心与宽高；目标位置通过修改各模块的 **`scenePos()`**（内部用 `moveBy` 与拖拽移动一致）写入 **`MoveItemsCommand`** 的起点/终点表，与「难点 5」共用同一命令类。
- **对齐**：左/右/上/下分别对齐到各边 **min(left)** / **max(right)** 等；**水平/垂直居中**则对齐到**选中集包围盒** `QRectF` 并集后的 `center()` 与各模块 `center()` 的差。
- **分布（平均间距）**：按 **中心坐标**排序后，取**最左与最右**（或最上与最下）的边作为总跨度 `span`，`gap = (span − 各块宽度或高度之和) / (n−1)`；若 **`gap < 0`**（总宽度已超过跨度），将 **`gap` 置 0**，避免强行分离重叠模块。
- **撤销文案**：`MoveItemsCommand` 支持可选描述字符串（如「Align left」「Distribute horizontally」），便于 **`QUndoView`** 与菜单撤销提示区分于普通「Move modules」。

## TCP / 串口下发（协议与实现要点）

应用层「协议」非常简单：**与 `SceneIo` 存盘相同的 JSON 对象**（`SceneIo::sceneToJsonBytes(..., compact=true)`），**UTF-8 编码，末尾追加一个换行符 `\n`**，便于对端按行解析。TCP 与串口仅传输载体不同。

### 实现上的难点与处理


| 难点                | 说明与处理                                                                                                                                                                        |
| ----------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **可选组件编译**        | 串口依赖 **Qt SerialPort**。CMake 用 `find_package` 检测 `Qt6::SerialPort` / `Qt5::SerialPort`；未找到时定义 `GRAPHICS_DEMO_HAS_SERIAL=0`，`PatchTransport::sendSerial` 在运行期返回明确错误，避免链接期硬依赖。 |
| **与 CMake 目标名一致** | 部分发行版包名为 `Qt6SerialPort`，需同时尝试 `find_package(Qt6 COMPONENTS SerialPort)` 与 `find_package(Qt6SerialPort)`，以适配不同安装布局。                                                          |
| **Linux 串口权限**    | 打开 `/dev/ttyUSB*`、`/dev/ttyACM*` 常出现 **Permission denied**（设备属组多为 `dialout`）。处理：将用户加入 `dialout` 并重新登录；程序在错误串包含 `Permission denied` 时追加一行提示。                                  |
| **串口「输入/输出错误」**   | 多为 **EIO**：USB 松动、设备掉线、端口被占用、线材/供电问题。属运行时环境与硬件问题，非应用层 JSON 格式错误；需在 UI 文案或文档中区分于「权限不足」。                                                                                       |
| **无分帧/无校验**       | 当前实现不做长度前缀或 CRC；大 JSON 或不可靠链路需对端自行做超时、重试或应用层校验。                                                                                                                              |
| **TCP 与串口错误语义不同** | TCP 侧常见为连接被拒、超时；串口侧常见为权限、EIO。调试时应先确认是哪一种通道。                                                                                                                                  |


## 多文档 Tab（多场景）实现要点与踩坑

主窗口用 `QTabWidget`，每个标签页一个 `DemoView`，各自持有 `**QGraphicsScene` + `QUndoStack`**；菜单栏撤销/重做通过 `**QUndoGroup**` 绑定到**当前标签**的活动栈（`setActiveStack` 随 `currentChanged` 切换）。

### 难点与处理


| 难点                            | 说明与处理                                                                                                                                                                               |
| ----------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **撤销/重做跨标签**                  | 每个标签独立 `QUndoStack`，若菜单仍绑定创建时的第一个栈，切换标签后撤销会作用在错误文档。使用 `**QUndoGroup::addStack` / `removeStack` / `setActiveStack`**，菜单动作用 `m_undoGroup->createUndoAction` / `createRedoAction` 创建。  |
| `**qobject_cast<DemoView*>**` | 从 `QTabWidget::widget(i)` 取回视图时，若需 `qobject_cast`，`DemoView` 必须带 `**Q_OBJECT**`，且 **CMake 需把 `include/DemoView.h` 列入目标源文件**，否则 AUTOMOC 不生成元对象，链接报 `vtable` / `staticMetaObject` 缺失。 |
| **Qt 版本与 `setClean`**         | Qt 6 的 `QUndoStack::setClean()` **无参数**；保存/加载成功后调用以标记「已与磁盘一致」。标签标题上的 `*` 依赖 `cleanChanged` 更新。                                                                                      |
| **关闭标签选「不保存」崩溃**              | 若先 `delete` 场景再销毁 `QUndoStack`，栈内 `QUndoCommand` 析构仍会访问已释放的 `ModuleItem` 等指针。**在 `~DemoView` 中必须先 `m_undoStack->clear()`，再 `setScene(nullptr)` 并 `delete` 场景。**                     |
| **文档路径与脏标记**                  | 每页独立 `documentPath()`；`promptSaveIfDirty` 在覆盖打开、关标签前调用；保存成功后 `setClean()` 并刷新标签文案与窗口标题。                                                                                             |


## 说明

该项目适合作为图形化流程编辑器（例如节点编排、模块连接器）的最小原型参考。

## TodoList（可补充的操作/功能）

- **更多图形交互**
  - 中键/右键平移画布
  - 缩放到选中对象、缩放复位（Fit to contents）
  - 网格背景与对齐网格（Snap）
- **项目级操作**
  - 导出图片（PNG/SVG）
- **端点与连线逻辑增强**
  - 输入端点连接约束（例如：一个输入只允许一条线）
  - 选中/悬停高亮关联模块与连线
  - 悬停提示（如 “Source → Filter”）

