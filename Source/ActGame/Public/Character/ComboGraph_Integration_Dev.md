# ComboGraph 接入教程

这份文档只描述当前项目里已经落地的链路，不讲插件原始示例。

目标是把一条输入链路接通到以下两种结果：

1. 命中 `ComboGraph` 命令绑定时，启动或推进连招图。
2. 没有命中 `ComboGraph` 时，回退到普通 GAS Ability 激活。

## 一、当前项目里的真实调用链

当前工程的完整链路如下：

1. `UActHeroComponent` 初始化角色输入与命令配置。
2. `AActPlayerController` 缓冲按键输入与方向输入，并驱动 `FActBattleInputAnalyzer`。
3. `FActBattleInputAnalyzer` 根据 `FInputCommandDefinition` 生成 `InputCommand.*` 命令标签。
4. `AActPlayerController::ResolveBufferedCommand` 取出命令并交给 `FActBattleCommandResolver`。
5. `FActBattleCommandResolver` 优先尝试 `UActComboGraphComponent`。
6. `UActComboGraphComponent` 根据 `UActComboGraphConfig` 决定是启动新图还是给当前图喂输入。
7. `UGameplayTask_StartComboGraph` 通过 ASC 激活 `UComboGraphNativeAbility`。
8. `UComboGraphNativeAbility` 创建 `UComboGraphAbilityTask_StartGraph`。
9. `UComboGraphAbilityTask_StartGraph` 负责实际节点推进、Montage 播放、窗口期和边输入处理。
10. 如果 `ComboGraph` 没处理该命令，解析器回退到 `UActAbilitySystemComponent::TryActivateGrantedAbilityByInputTag`。

## 二、你需要配置的资产

当前项目里至少有三层配置：

1. `UActPawnData`
   路径参考：[ActPawnData.h](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Public\Character\ActPawnData.h)
2. `UActInputCommandConfig`
   路径参考：[ActInputCommandConfig.h](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Public\Input\ActInputCommandConfig.h)
3. `UActComboGraphConfig`
   路径参考：[ActComboGraphConfig.h](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Public\Character\ActComboGraphConfig.h)

在 `PawnData` 里需要挂上：

1. `InputConfig`
   负责 Enhanced Input 与 `InputTag.*` 的映射。
2. `InputCommandConfig`
   负责把输入历史翻译成 `InputCommand.*`。
3. `ComboGraphConfig`
   负责把 `InputCommand.*` 绑定到具体 `ComboGraph` 和入口 `InputAction`。

## 三、输入系统怎么进入命令分析器

入口在 [ActHeroComponent.cpp](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Private\Character\ActHeroComponent.cpp)。

关键点如下：

1. `InitializePlayerInput`
   会从 `PawnData` 读取 `InputCommandConfig`，再调用 `ActPC->ConfigureInputCommandDefinitions(...)`。
2. `Input_AbilityInputTagPressed`
   不直接激活 Ability，而是优先调用 `ActPC->QueueAbilityInputTagPressed(InputTag)`。
3. `Input_Move`
   会把角色相对方向量化成 `EInputDirection`，再调用 `ActPC->PushDirectionToAnalyzer(...)`。

这意味着命令分析器吃的是两类输入：

1. `InputTag.*`
   比如轻攻击、跳跃。
2. 角色相对方向
   比如 `Forward`、`BackwardRight`。

## 四、命令定义怎么写

命令定义结构在 [ActBattleInputAnalyzer.h](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Public\Input\ActBattleInputAnalyzer.h)。

核心数据结构：

1. `FInputCommandStep`
   描述一步输入。
2. `FInputCommandDefinition`
   描述一整条命令以及最终输出标签。

一个命令至少包含：

1. `OutputCommandTag`
   命令匹配成功后输出的标签，建议使用 `InputCommand.*`。
2. `Priority`
   多条命令同时命中时，优先级高的获胜。
3. `BufferLifetimeSeconds`
   匹配后可在缓冲区保留多久。
4. `Steps`
   输入步骤数组。

例子一，普通轻攻击命令：

```text
OutputCommandTag = InputCommand.Attack.Light
Steps:
- Step 1: InputTag = InputTag.Attack.Light, EventType = Pressed, RequiredDirection = Any
```

例子二，前前轻攻击命令：

```text
OutputCommandTag = InputCommand.Attack.DashLight
Steps:
- Step 1: RequiredDirection = Forward
- Step 2: RequiredDirection = Forward
- Step 3: InputTag = InputTag.Attack.Light, EventType = Pressed
```

注意：

1. 方向步骤可以只填 `RequiredDirection`，不一定非要有 `InputTag`。
2. `AllowedTimeGap` 控制每一步之间的最大时间差。
3. `RequiredStateTags` 可以限制例如空中、地面、锁定等前置状态。

## 五、命令如何进入 ComboGraph

命令解析入口在 [ActBattleCommandResolver.cpp](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Private\Input\ActBattleCommandResolver.cpp)。

处理顺序是固定的：

1. 先拿到 Avatar 上的 `UActComboGraphComponent`。
2. 调用 `ComboGraphComp->TryHandleCommand(CommandTag)`。
3. 如果当前有活跃 `ComboGraph` 且成功处理，则本次命令到此结束。
4. 如果当前存在活跃 `ComboGraph` 但这次没被它处理，则不立即回退到普通 Ability。
5. 如果没有活跃 `ComboGraph`，再走 `ActASC.TryActivateGrantedAbilityByInputTag(CommandTag, ...)`。

这套顺序保证了：

1. 连招过程中，命令优先服务于当前连招图。
2. 空闲状态下，同一个命令标签也可以直接作为 Ability 输入标签使用。

## 六、ComboGraph 绑定怎么写

绑定结构在 [ActComboGraphConfig.h](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Public\Character\ActComboGraphConfig.h)。

每一条 `FActComboGraphCommandBinding` 包含：

1. `CommandTag`
   一般填 `InputCommand.*`。
2. `ComboGraph`
   要启动的图资产。
3. `InputAction`
   启动图时传给入口 Conduit 或后续边判断的 `UInputAction`。

运行组件在 [ActComboGraphComponent.cpp](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Private\Character\ActComboGraphComponent.cpp)。

行为规则如下：

1. `TryHandleCommand`
   先查 `CommandTag` 对应绑定。
2. 当前没有活跃图时
   调用 `StartComboGraphForBinding`。
3. 当前已经有活跃图时
   调用 `FeedActiveComboGraph`。

`FeedActiveComboGraph` 内部最终做的是：

1. `UComboGraphBlueprintLibrary::SimulateComboInput(GetOwner(), Binding.InputAction)`

也就是说，命令标签只是项目侧的中间层映射键，真正喂给 `ComboGraph` 的仍然是 `UInputAction`。

## 七、GameplayTask 与 GA 是怎么接上的

启动图的桥接层在 [GameplayTask_StartComboGraph.cpp](D:\Workspace\UnrealEngine\Projects\ActionGame\Plugins\ComboGraph\Source\ComboGraph\Private\Abilities\Tasks\GameplayTask_StartComboGraph.cpp)。

它做的事情很简单：

1. 从 Avatar 找 ASC。
2. 找到已授予的 `UComboGraphNativeAbility`。
3. 用 `FGameplayEventData` 把两个对象传下去：
   `OptionalObject = ComboGraph`
   `OptionalObject2 = InitialInput`
4. 调用 `ASC->InternalTryActivateAbility(...)` 激活原生 GA。

原生 Ability 在 [ComboGraphNativeAbility.cpp](D:\Workspace\UnrealEngine\Projects\ActionGame\Plugins\ComboGraph\Source\ComboGraph\Private\Abilities\ComboGraphNativeAbility.cpp)。

它负责：

1. 从 `TriggerEventData` 取出 `ComboGraph` 和 `InputAction`。
2. `CommitAbility`。
3. 创建 `UComboGraphAbilityTask_StartGraph`。
4. 把 `OnGraphEnd` 和 `EventReceived` 转发出来。

所以当前项目不是“输入直接驱动插件”，而是：

`项目输入系统 -> 项目命令层 -> 项目 ComboGraph 组件 -> GameplayTask -> Native GA -> ComboGraph Runtime`

## 八、实操例子：做一个普通 1A 起手连招

目标：

1. 按一次轻攻击，启动 `CG_LightAttack`。
2. 在窗口期再次按轻攻击，进入第二段。

操作步骤：

1. 在 `InputConfig` 里确认轻攻击对应一个 `InputAction`
   例如 `IA_AttackLight`。
2. 在 `DefaultGameplayTags.ini` 或标签资产中准备标签
   `InputTag.Attack.Light`
   `InputCommand.Attack.Light`
3. 在 `UActInputCommandConfig` 里新增一条命令
   `OutputCommandTag = InputCommand.Attack.Light`
   单步输入为 `InputTag.Attack.Light`
4. 创建一个 `ComboGraph` 资产
   例如 `CG_LightAttack`
5. 在图里配置入口节点或入口 Conduit
   如果第一步依赖入口输入，请让边的 `TransitionInput` 指向 `IA_AttackLight`
6. 在第二段边上同样配置 `TransitionInput = IA_AttackLight`
7. 在 `UActComboGraphConfig` 里新增一条绑定
   `CommandTag = InputCommand.Attack.Light`
   `ComboGraph = CG_LightAttack`
   `InputAction = IA_AttackLight`
8. 在角色使用的 `PawnData` 上挂好这份 `ComboGraphConfig`
9. 确认角色 ASC 已经授予 `UComboGraphNativeAbility`

运行效果：

1. 第一次轻攻击命令会启动图。
2. 图活跃后，再次命中同一命令时，不会重新开图。
3. `UActComboGraphComponent` 会把这次输入转成 `SimulateComboInput`，驱动当前图走边。

## 九、实操例子：同一输入先给 ComboGraph，不命中再回退 GA

这是当前工程默认支持的模式。

假设：

1. `InputCommand.Jump` 没有绑定到 `ComboGraph`。
2. ASC 里存在一个输入标签也叫 `InputCommand.Jump` 的 Ability。

结果：

1. 解析器先尝试 `ComboGraph`。
2. `ComboGraph` 没处理时，自动回退到 `TryActivateGrantedAbilityByInputTag(InputCommand.Jump)`。

适用场景：

1. 某些输入在连招图里是派生招式。
2. 同样的输入在空闲态又是普通能力。

## 十、连招中间层各层职责怎么分

建议以后继续保持这几个边界，不要重新耦合回去：

1. `UActHeroComponent`
   只负责采样输入、配置输入、把输入交给控制器。
2. `AActPlayerController`
   只负责输入缓冲、方向量化后的命令分析、命令消费。
3. `FActBattleCommandResolver`
   只负责“这条命令交给谁处理”的决策。
4. `UActComboGraphComponent`
   只负责项目层的命令标签到 `ComboGraph` / `InputAction` 映射。
5. `UGameplayTask_StartComboGraph`
   只负责把项目层请求转成 GAS 激活。
6. `UComboGraphNativeAbility`
   只负责承接 GAS 激活，并创建真正的运行时任务。
7. `UComboGraphAbilityTask_StartGraph`
   只负责插件内的运行时状态机和节点推进。

## 十一、排查建议

如果输入按了没反应，按下面顺序查：

1. `PawnData` 是否挂了 `InputCommandConfig` 和 `ComboGraphConfig`。
2. `ActHeroComponent::InitializePlayerInput` 是否在运行时被调用。
3. `QueueAbilityInputTagPressed` 是否有日志。
4. `HandleInputCommandMatched` 是否真的匹配到了 `InputCommand.*`。
5. `ActBattleCommandResolver` 是否先把命令交给了 `ComboGraph`。
6. `ActComboGraphComponent::FindBinding` 是否能查到该命令绑定。
7. 绑定里的 `InputAction` 是否和图里的 `TransitionInput` 用的是同一个资产。
8. ASC 是否授予了 `UComboGraphNativeAbility`。
9. `UGameplayTask_StartComboGraph` 是否成功找到现有 AbilitySpec。
10. 图的入口节点、Conduit 边、节点动画是否配置完整。

## 十二、当前项目里最关键的几个文件

项目侧：

1. [ActPawnData.h](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Public\Character\ActPawnData.h)
2. [ActInputCommandConfig.h](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Public\Input\ActInputCommandConfig.h)
3. [ActBattleInputAnalyzer.h](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Public\Input\ActBattleInputAnalyzer.h)
4. [ActHeroComponent.cpp](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Private\Character\ActHeroComponent.cpp)
5. [ActPlayerController.cpp](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Private\Player\ActPlayerController.cpp)
6. [ActBattleCommandResolver.cpp](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Private\Input\ActBattleCommandResolver.cpp)
7. [ActComboGraphConfig.h](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Public\Character\ActComboGraphConfig.h)
8. [ActComboGraphComponent.cpp](D:\Workspace\UnrealEngine\Projects\ActionGame\Source\ActGame\Private\Character\ActComboGraphComponent.cpp)

插件侧：

1. [GameplayTask_StartComboGraph.cpp](D:\Workspace\UnrealEngine\Projects\ActionGame\Plugins\ComboGraph\Source\ComboGraph\Private\Abilities\Tasks\GameplayTask_StartComboGraph.cpp)
2. [ComboGraphNativeAbility.cpp](D:\Workspace\UnrealEngine\Projects\ActionGame\Plugins\ComboGraph\Source\ComboGraph\Private\Abilities\ComboGraphNativeAbility.cpp)
3. [ComboGraphAbilityTask_StartGraph.cpp](D:\Workspace\UnrealEngine\Projects\ActionGame\Plugins\ComboGraph\Source\ComboGraph\Private\Abilities\Tasks\ComboGraphAbilityTask_StartGraph.cpp)

如果你后面继续扩展连招中间层，优先改项目侧三个点：

1. `FInputCommandDefinition`
2. `FActBattleCommandResolver`
3. `UActComboGraphConfig`

这样不会把项目规则硬编码进插件运行时。
