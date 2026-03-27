# AbilityChain（从离散蒙太奇到完整派生连招）

## 一、系统结构与职责

AbilityChain 分为三层：

1. `Input`
   - `FActBattleInputAnalyzer` 负责输入匹配与缓存
   - `FActBattleCommandResolver` 只负责把命令转给 ASC 链路
2. `AbilityChain`
   - `FActAbilityChainRuntime` 负责节点状态、窗口状态、预测宽限、弱网对齐
   - `UActAbilityChainData` 是唯一的连招配置资产
3. `Ability`
   - `UActGameplayAbility` 上配置 `AbilityChainData` / `AbilityChainActivationMode`
   - `UAT_PlayMontageAndWaitForEvent` 在 `StartSection` 为空时自动用链路的起始节点

核心要点：连招权威在 ASC Runtime，不在 AN/ANS。AN/ANS 只是事件/窗口信号。

## 二、从离散蒙太奇开始：准备与整理

适用场景：你有多个“离散 Montage”或“单段 Sequence”，希望拼成完整连招。

1. 合并蒙太奇
   - 把同一套连招段落合并到一个 Montage
   - 每个连招步骤对应一个 Montage Section
   - Section 名字必须稳定、唯一，例如 `Attack_01` / `Attack_02` / `Attack_03`

2. 放置节点 Notify（每段必放）
   - 在每个 Section 开头放 `AN_AbilityChainNode`（定义在 `ActAbilityChainNotifies.h`）
   - `NodeId` 必须和链路配置里的 `NodeId` 一致
   - 一段 Section 只需要一个 `AN_AbilityChainNode`

3. 放置窗口 NotifyState（控制可接与取消）
   - 在“可接/可取消”区间放 `ANS_AbilityChainWindow`（定义在 `ActAbilityChainNotifies.h`）
   - `WindowTag` 建议统一命名，例如：
     - `AbilityChain.Window.Combo.Light`
     - `AbilityChain.Window.Combo.Heavy`
     - `AbilityChain.Window.Cancel.Jump`
     - `AbilityChain.Window.Cancel.Dodge`

4. 伤害、特效、命中时点保持原有方式
   - 原有 `AnimNotify` / `GameplayEvent` 不动
   - AbilityChain 只负责“能不能接，接到哪里”

## 三、创建 AbilityChain 配置资产

创建 `UActAbilityChainData`（一套连招树一个资产）。

### 关键字段

- `StartNodeId`：起始节点
- `PredictionGraceSeconds`：预测宽限时长，建议 `0.35~0.5`
- `Nodes`：连招节点表

### Node 字段

- `NodeId`：与 `AN_AbilityChainNode` 一一对应
- `SectionName`：Montage 内 Section 名字
- `Transitions`：从该节点出发的分支

### Transition 字段

- `CommandTag`：输入命令（来自 `InputAnalyzer`）
- `RequiredWindowTags`：该分支必须打开的窗口
- `BlockedWindowTags`：该分支禁止打开的窗口
- `RequiredOwnerTags` / `BlockedOwnerTags`：状态标签限制
- `Priority`：多条命令同时符合时，优先级越高越先匹配
- `bConsumeInput`：命中后是否消耗输入缓冲
- `bCancelCurrentAbility`：后摇取消/跨 Ability 跳转时启用
- `TransitionType`：`Section` 或 `Ability`
- `TargetNodeId`：Section 跳转的目标节点
- `TargetAbilityID`：跨 Ability 跳转的目标 AbilityID

## 四、Ability 配置

每个连招相关的 Ability 必须配置：

- `AbilityID`
- `AbilityChainActivationMode`
- `AbilityChainData`（仅在“拥有链路的能力”上配置）

推荐模式：

- `StarterOnly`：只能作为起手
- `ChainOnly`：只能作为连招派生
- `StarterOrChain`：既可起手也可派生
- `Ignore`：完全不参与链路

注意：

- 派生/取消能力一般设置为 `ChainOnly`
- 普通起手能力一般设置为 `StarterOnly` 或 `StarterOrChain`

## 五、一步步做出完整派生连招

例子：`1A -> 2A -> 3A`，并且在 `2A/3A` 允许 `Jump` 取消。

1. Montage 里做 3 个 Section
   - `Attack_01` / `Attack_02` / `Attack_03`
   - 每个 Section 开头放 `AN_AbilityChainNode`

2. 窗口配置
   - `Attack_01` 后半段放 `AbilityChain.Window.Combo.Light`
   - `Attack_02` 后半段放 `AbilityChain.Window.Combo.Light`
   - `Attack_02` 后摇段放 `AbilityChain.Window.Cancel.Jump`

3. AbilityChainData 配置
   - Node `Attack_01`
     - `InputCommand.Attack.Light` -> `Attack_02`
   - Node `Attack_02`
     - `InputCommand.Attack.Light` -> `Attack_03`
     - `InputCommand.Jump` -> `AbilityID=Attack_Launch`, `bCancelCurrentAbility=true`
   - Node `Attack_03`
     - `InputCommand.Jump` -> `AbilityID=Attack_Launch`, `bCancelCurrentAbility=true`

4. Ability 配置
   - `GA_Combo_Attack`：`AbilityChainData=你的链路资产`
   - `GA_Attack_Launch`：`AbilityChainActivationMode=ChainOnly`

效果：

- 连招段通过 Section 跳转
- 取消段通过 Ability 跳转
- `bCancelCurrentAbility` 负责打断后摇

## 六、多窗口机制（同一节点多窗口）

同一节点可以有多个窗口：

- `WindowA`（轻击接招窗口）
- `WindowB`（重击派生窗口）
- `WindowC`（取消窗口）

配置要点：

- 每条 Transition 指明 `RequiredWindowTags`
- 多窗口可叠加：`RequiredWindowTags` 可放多个，必须全满足
- `BlockedWindowTags` 用于排除冲突窗口
- 如果多个分支都满足，使用 `Priority` 决定哪条生效

## 七、取消机制（Recovery Cancel）

取消机制完全依赖 Transition：

- `TransitionType=Ability`
- `bCancelCurrentAbility=true`
- `RequiredWindowTags` 指定取消窗口
- `TargetAbilityID` 指向取消能力

注意：

- 取消目标 Ability 的 `ActivationMode` 不能是 `StarterOnly`
- 取消窗口建议比连招窗口更短，以避免误触发
- 取消能力既可独立 GA，也可以共享 GA

## 八、蓝图里要不要做额外处理

一般不需要额外蓝图逻辑，只做配置：

- 在 GA 蓝图里设置
  - `AbilityID`
  - `AbilityChainData`
  - `AbilityChainActivationMode`
- 在 Montage 里放
- `AN_AbilityChainNode`
- `ANS_AbilityChainWindow`

## 附：动作取消窗口（Cancel Window）
如果你希望“移动/闪避/跳跃/技能”等机制可以在某段动画区间打断当前技能，不要用锁输入的方式做。
推荐在动画上放 `ANS_CancelWindow`（定义在 `Character/ActCharacterMovementNotifies.h`），它只标记“可取消区间”，不阻止移动输入。
- 在 GA 执行中用 `PlayMontageAndWaitForEvent`
  - `StartSection` ~~留空即可自动从起始节点播放~~

不要在蓝图中手动控制窗口开关，否则会破坏弱网一致性。

## 九、弱网对齐与 400ms 关键点

链路弱网稳定依赖三件事：

- GAS 本地预测
- `PredictionGraceSeconds` 宽限
- 客户端把“已预测分支结果”同步给服务器
  - 服务器按 `SourceNode -> TargetNode/Ability` 应用
  - 不再依赖服务器自己的本地窗口时间

确保以下条件成立：

- 节点和窗口都正确放置
- `PredictionGraceSeconds` 有合理值
- `ActivationMode` 设置正确
- 取消能力允许链路激活

## 十、常见问题排查

1. 连招不接
   - 是否进入了正确的 `NodeId`
   - 窗口 Tag 是否打开
   - Transition 是否配置正确

2. 取消不生效
   - `bCancelCurrentAbility` 是否勾选
   - 取消 Ability 是否 `ChainOnly` 或 `StarterOrChain`

3. 弱网抖动
   - `PredictionGraceSeconds` 是否太小
   - 是否仍在用旧 `ANS_AbilityChain` 资产
