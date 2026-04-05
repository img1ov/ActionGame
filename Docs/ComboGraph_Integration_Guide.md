# ComboGraph 接入教程

这份文档对应当前仓库里的实现，前提边界如下：

- 不兼容旧资产，`ComboGraph` / `Transition` / `Window` 资产按新规则重建。
- `WeaponGA` / `ComboGA` 才是正式启动点。
- `UActComboGraphComponent` 只负责转发输入和查询状态，不负责持有主运行时。
- `ActGame` 和 `ComboGraph` 只通过插件公开 API 交互，插件单独拿出来也能跑。

---

## 1. 当前主体链路

推荐的正式链路是：

1. `WeaponGA` 或你的 `ComboGA` 在合适时机调用 `UComboGraphAbilityTask_StartGraph::CreateStartComboGraph(...)`
2. `ComboGraphAbilityTask_StartGraph` 负责：
   - 入口 `Conduit` 分流
   - `Command / InputAction` 二选一匹配
   - `WindowTags + Priority` 裁决
   - `TransitionPriority` 裁决
   - Montage 播放与推进
3. 输入层只负责把：
   - `CommandTag`
   - 或 `InputAction`
   转发给运行中的图
4. 如果当前图不接这个输入，再回退给普通 GAS Ability 输入

现在已经补好的关键点：

- `TransitionPolicy` 严格收敛为 `Command` / `InputAction`
- `Window` 运行时改为 `ActiveWindows`
- `Window` 只做 `WindowTags + Priority`
- `Alias` 是纯编辑器节点，编译时展开，runtime 无感知
- `ActComboGraphComponent` 改成薄转发层
- 提供了 runtime 重置接口
- 提供了当前节点 / 活跃窗口查询接口，方便后续 Debug UI

---

## 2. 你真正该用的启动入口

### 正式入口

正式建议只用能力任务：

- [ComboGraphAbilityTask_StartGraph.h](D:/Workspace/UnrealEngine/Projects/ActionGame/Plugins/ComboGraph/Source/ComboGraph/Public/Abilities/Tasks/ComboGraphAbilityTask_StartGraph.h)
- [ComboGraphAbilityTask_StartGraph.cpp](D:/Workspace/UnrealEngine/Projects/ActionGame/Plugins/ComboGraph/Source/ComboGraph/Private/Abilities/Tasks/ComboGraphAbilityTask_StartGraph.cpp)

典型做法：

```cpp
UComboGraphAbilityTask_StartGraph* Task =
	UComboGraphAbilityTask_StartGraph::CreateStartComboGraph(
		this,
		ComboGraphAsset,
		InitialInputAction,
		InitialCommandTag,
		/*bBroadcastInternalEvents=*/true);

Task->ReadyForActivation();
```

说明：

- `InitialInputAction` / `InitialCommandTag` 主要用于入口 `Conduit`
- 如果入口没有 `Conduit`，这两个只是在首段分流时无效，不影响后续运行
- 你可以在自定义 `WeaponGA` / `ComboGA` 里直接持有这个任务引用

### 非主路径入口

`UActComboGraphComponent::StartComboGraph` / `StartComboGraphWithCommand` 还保留着，但只是便捷包装：

- [ActComboGraphComponent.h](D:/Workspace/UnrealEngine/Projects/ActionGame/Source/ActGame/Public/Character/ActComboGraphComponent.h)
- [ActComboGraphComponent.cpp](D:/Workspace/UnrealEngine/Projects/ActionGame/Source/ActGame/Private/Character/ActComboGraphComponent.cpp)

不建议把它作为正式启动方式。你的边界是对的：主启动口应该在 GA。

---

## 3. 输入该怎么接

### 3.1 Command 输入

项目里的命令解析入口还是：

- [ActBattleCommandResolver.cpp](D:/Workspace/UnrealEngine/Projects/ActionGame/Source/ActGame/Private/Input/ActBattleCommandResolver.cpp)

当前逻辑：

1. 先查角色身上的 `UActComboGraphComponent`
2. 如果运行中的图能处理当前 `CommandTag`，就转发给图
3. 否则回退到 `ASC.TryActivateGrantedAbilityByInputTag(...)`

这保证了两件事能同时成立：

- 连招图在跑时，可以优先吃掉 Combo 命令
- 没被图消费的输入，仍然能走普通 GAS Ability

### 3.2 InputAction 输入

如果你也要支持原生 `InputAction` 直连图，直接转发到：

- `UActComboGraphComponent::SendComboGraphInput`
- 或插件侧 `UComboGraphBlueprintLibrary::SimulateComboInput`

公开查询/转发 API 在：

- [ComboGraphBlueprintLibrary.h](D:/Workspace/UnrealEngine/Projects/ActionGame/Plugins/ComboGraph/Source/ComboGraph/Public/Utils/ComboGraphBlueprintLibrary.h)
- [ComboGraphBlueprintLibrary.cpp](D:/Workspace/UnrealEngine/Projects/ActionGame/Plugins/ComboGraph/Source/ComboGraph/Private/Utils/ComboGraphBlueprintLibrary.cpp)

新增的关键接口：

- `GetActiveComboGraphTask`
- `HasActiveComboGraph`
- `CanHandleActiveComboCommand`
- `RestartActiveComboGraph`
- `GetActiveComboGraphNode`
- `GetActiveComboGraphWindows`

---

## 4. 重置连招怎么做

现在已经补了逻辑重置接口，语义是：

- 中断当前连招
- 停掉当前 Montage
- 清空当前运行时窗口
- 回到图入口重新选起手

可以从两层调用：

1. 插件层：
   - `UComboGraphBlueprintLibrary::RestartActiveComboGraph(Actor)`
2. 项目层：
   - `UActComboGraphComponent::ResetComboGraph()`

如果你是在 `WeaponGA` / `ComboGA` 里做强制回起手，建议直接走插件层或任务实例本身。

---

## 5. Transition / Window 规则

### Transition

每条边现在按下面规则工作：

- `TransitionPolicy = Command` 或 `InputAction`
- `TransitionPriority`
- `RequiredWindowTags`

运行时裁决顺序：

1. 先筛输入命中的边
2. 再筛 `RequiredWindowTags`
3. 先比命中的 `WindowPriority`
4. 再比 `TransitionPriority`
5. 最后才退回图内边顺序

### Window

当前运行时窗口是集合，不再是单布尔：

- `ActiveWindows`

每个窗口只保留：

- `WindowTags`
- `Priority`

对应 Notify：

- [ComboGraphANS_ComboWindow.h](D:/Workspace/UnrealEngine/Projects/ActionGame/Plugins/ComboGraph/Source/ComboGraph/Public/AnimNotifies/ComboGraphANS_ComboWindow.h)
- [ComboGraphANS_ComboWindow.cpp](D:/Workspace/UnrealEngine/Projects/ActionGame/Plugins/ComboGraph/Source/ComboGraph/Private/AnimNotifies/ComboGraphANS_ComboWindow.cpp)

这和你现在要求的“只按 Tag + 优先级排序，保持简单”是一致的。

---

## 6. Alias 现在怎么用

Alias 已经改成更接近动画状态机 Alias 的编辑器语义：

- 可以空白处右键创建
- 也可以从一个普通节点拉出再创建，自动把该节点加入来源
- 来源节点不再靠输入 pin 维系
- 在 Details 面板里勾选哪些节点属于 Alias 来源
- Alias 本身仍然只在编译期展开

相关实现：

- [ComboGraphEdNodeAlias.h](D:/Workspace/UnrealEngine/Projects/ActionGame/Plugins/ComboGraph/Source/ComboGraphEditor/Public/Graph/Nodes/ComboGraphEdNodeAlias.h)
- [ComboGraphEdNodeAlias.cpp](D:/Workspace/UnrealEngine/Projects/ActionGame/Plugins/ComboGraph/Source/ComboGraphEditor/Private/Graph/Nodes/ComboGraphEdNodeAlias.cpp)
- [ComboGraphEdGraph.cpp](D:/Workspace/UnrealEngine/Projects/ActionGame/Plugins/ComboGraph/Source/ComboGraphEditor/Private/Graph/ComboGraphEdGraph.cpp)
- [ComboGraphSchema.cpp](D:/Workspace/UnrealEngine/Projects/ActionGame/Plugins/ComboGraph/Source/ComboGraphEditor/Private/Graph/ComboGraphSchema.cpp)
- [ComboGraphEdNodeAliasDetails.cpp](D:/Workspace/UnrealEngine/Projects/ActionGame/Plugins/ComboGraph/Source/ComboGraphEditor/Private/Details/ComboGraphEdNodeAliasDetails.cpp)

使用方式：

1. 创建一个 `Alias`
2. 在 `Details > Alias Sources` 里勾选多个来源节点
3. 从 `Alias` 输出连到目标节点
4. 给这些边配共享的 `Transition` 配置
5. Rebuild Graph 时会展开成普通 runtime 边

注意：

- `Alias` 不是 runtime 节点
- 不要拿它替代入口 `Conduit`
- 它表达的是“多个来源共享同一组目标转换”

---

## 7. 编辑器表现层现在的规则

### Transition 边显示

现在边上的显示规则已经调整为：

- `Command`：显示 `CMD`，并显示完整 GameplayTag
- `InputAction`：显示 `IA`，只显示图标，不再显示文本
- `P:x`：显示 `TransitionPriority`
- `W:...`：显示窗口需求摘要

相关文件：

- [ComboGraphEdNodeEdge.cpp](D:/Workspace/UnrealEngine/Projects/ActionGame/Plugins/ComboGraph/Source/ComboGraphEditor/Private/Graph/Nodes/ComboGraphEdNodeEdge.cpp)
- [SComboGraphEdge.cpp](D:/Workspace/UnrealEngine/Projects/ActionGame/Plugins/ComboGraph/Source/ComboGraphEditor/Private/Graph/Slate/SComboGraphEdge.cpp)

这次也顺手修了 IA 图标的拉伸问题，图标现在走固定尺寸盒子。

### Alias 外观

这次也修了两点：

- Alias 使用 `Graph.AliasNode.Icon`
- 因为不再保留输入 pin，左侧那种奇怪白板/白点观感会收敛很多

---

## 8. Debug 层现在能挂什么

虽然完整可视化 Debug 面板还没做完，但 runtime 钩子已经有了：

- 当前任务：`GetActiveComboGraphTask`
- 当前节点：`GetActiveComboGraphNode`
- 活跃窗口：`GetActiveComboGraphWindows`
- 当前图是否存在：`HasActiveComboGraph`
- 当前命令是否可被图消费：`CanHandleActiveComboCommand`

所以后面你做调试面板时，最少可以先把下面几项挂出来：

- 当前活跃节点
- 当前活跃窗口列表
- 当前是否有图在跑
- 当前一次命令是否会被图吃掉

如果再往前走一步，下一阶段最值得补的是：

- 候选 Transition 列表
- 最终命中哪条边
- 其他边为什么没命中

---

## 9. 资产侧重建建议

既然你明确说不兼容旧资产，重建时建议直接按下面规则做：

1. 所有边都显式设置 `TransitionPolicy`
2. `Command` 边直接填完整 `InputCommand.*` Tag
3. `InputAction` 边只用于真正想吃 Enhanced Input 的地方
4. `ComboWindow` notify 统一填：
   - `WindowTags`
   - `Priority`
5. 需要共享转换的地方统一用 `Alias`
6. `Conduit` 只留给入口首段分流，不要泛化成共享过渡节点

---

## 10. 当前还没补完的点

主体链路现在能跑，但还有两类内容值得继续做：

### 建议优先继续补

- 可视化 Debug 面板
- 候选边和失败原因可视化
- 当前窗口高亮
- 当前节点高亮

### 非阻塞但值得继续打磨

- Alias 节点视觉再细调
- 更多编辑器 Hover/Tooltip 信息
- 5.7 的 `FVector2f` 旧 API 警告清理

---

## 11. 一套推荐接法

如果按你的玩法结构来，建议最终稳定接法就是：

1. `WeaponGA` 决定当前该跑哪张 `ComboGraph`
2. `WeaponGA` / `ComboGA` 里直接创建 `UComboGraphAbilityTask_StartGraph`
3. 输入系统继续产出：
   - `CommandTag`
   - 或 `InputAction`
4. `UActComboGraphComponent` 只做：
   - `SendComboGraphCommand`
   - `SendComboGraphInput`
   - `ResetComboGraph`
   - 查询当前图状态
5. `ActBattleCommandResolver` 保持“图优先，Ability 回退”
6. 图内部按 `Policy / Window / Priority` 做最终裁决

这套分层和你现在的设计目标是完全一致的。
