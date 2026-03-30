# ActionGame 移动框架设计及工程实践手册

## 1. 文档目的

这份手册面向项目内部开发者，说明当前 `Character` 目录下这套移动框架为什么存在、在解决什么问题、为什么不能只依赖引擎原生能力，以及在工程里应该如何正确使用。

本文重点覆盖：

- 设计意图：这套框架到底在对抗引擎原生/GAS 的什么问题
- 架构原因：为什么要把输入、位移、旋转、蒙太奇、网络同步拆开
- 执行链：`Move`、`Motion`、`RotationTask`、`RootMotion 提取`、`弱网同步`
- 接口说明：`UActCharacterMovementComponent` 与几个 Ability Task 的使用方式
- 工程规则：什么场景该用，什么场景不要用

本文基于当前项目源码实现整理，不是脱离项目的泛化动作游戏理论。

---

## 2. 先给结论：这套框架最核心在解决什么

### 2.1 最核心问题

这套框架最核心解决的是：

**GAS 下带位移蒙太奇在联机环境中的回滚、拉位置、拉进度问题。**

动作游戏要求：

- 本地按下按钮必须立刻看到动作和位移反馈
- 弱网下不能因为服务器校正把角色猛地拉回去
- 不能因为位置回滚导致蒙太奇进度、连招窗口、受击反馈一起错位

如果直接依赖引擎原生的 `Montage + RootMotion + CharacterMovement 校正`：

- 客户端本地先播了带位移的攻击蒙太奇
- 服务器权威位置不同步
- 网络纠正会把位置拉回
- 如果 gameplay 又依赖动画实例进度、section、notify 时间，那么表现和判定会一起漂

这对动作游戏来说是致命问题。

所以本项目的核心思路是：

**把“蒙太奇播放”和“位移执行权”拆开。**

- 蒙太奇负责表现、事件、section 切换
- 位移由 `Motion` 层执行
- 如果动画里已经有 RootMotion，不直接让原生系统跑，而是“提取 RootMotion 到 Motion”
- Motion 再进入预测、同步、回放、校正体系

这样做的目标不是让代码更复杂，而是为了保证：

- 本地表现优先
- 服务器仍然权威
- 预测和回滚不是“回滚裸位置”，而是“对齐同一条运动过程”

### 2.2 次级问题

除了 GAS 蒙太奇位移回滚，这套框架还同时解决了几个动作游戏里的工程问题：

1. **基础移动和战斗位移分层**
   - 跑步、刹车、落地、坡面、跳跃继续交给 UE `CharacterMovement`
   - 冲刺、攻击前冲、击退、击飞、锁敌转向交给自定义 `Motion`

2. **程序位移和动画位移统一抽象**
   - 显式速度位移
   - Launch/击飞
   - 旋转任务
   - 从蒙太奇提取的 RootMotion
   - 都进入同一套 `MotionStateMap`

3. **多来源旋转冲突仲裁**
   - 锁敌转向
   - 攻击转向
   - 受击转向
   - RootMotion 自带旋转
   - 通过优先级与暂停机制统一处理

4. **联机下位移可预测、可复制、可对齐**
   - 客户端保存预测 Motion 快照
   - 服务端复制权威 Motion 状态
   - 模拟端按 Motion 状态补追，而不是只拿最终位置硬拉

5. **输入阻断时不丢玩家意图**
   - 动画/技能期间可以禁止 `AddMovementInput`
   - 但玩家输入方向仍保留给指令分析器和连招系统

---

## 3. 为什么引擎自带还要这么设计

### 3.1 引擎原生能力擅长什么

UE 原生的 `CharacterMovementComponent` 很适合处理：

- 基础地面移动
- 加速度/减速度
- 碰撞与滑动
- 楼梯、斜坡、落地
- 跳跃、下落
- 普通网络平滑和位置纠正

这些能力项目没有重复造轮子，而是保留并继续使用。

### 3.2 引擎原生能力不擅长什么

原生能力不擅长的是动作游戏里这类位移：

- 攻击前冲
- 技能位移
- 受击后退
- 击飞
- 朝目标转身
- 锁敌移动时自动朝向目标
- 带 RootMotion 的攻击在弱网环境下保持本地表现优先

问题不在于引擎“做不到动”，而在于它默认不关心下面这些动作游戏约束：

1. **本地表现必须先于服务器确认**
2. **位移不能完全绑定动画实例状态**
3. **弱网回滚不能破坏战斗节奏**
4. **不同来源位移要能统一堆叠和仲裁**
5. **技能位移要进入 GAS 预测/同步链路**

### 3.3 为什么不能直接靠原生 RootMotion

原生 RootMotion 最大的问题不是“精度”，而是“控制权归属”。

如果 gameplay 位移完全交给动画实例：

- 位移生命周期绑在 montage runtime 上
- 进度绑定当前动画时间
- 网络回滚时，位置和动画时间容易一起漂
- 受击/连招/取消窗口如果也跟着 montage time 走，弱网下会非常不稳定

因此本项目的原则是：

**原生 RootMotion 可以是数据来源，但不应该默认成为 gameplay 位移执行权。**

这就是 `ExtractedRootMotion` 模式存在的原因。

---

## 4. 框架总览

当前移动框架可以分成 6 层。

### 4.1 输入层

主要类：

- `UActHeroComponent`

职责：

- 接收输入
- 计算控制器空间到世界空间的移动方向
- 调用 `AddMovementInput`
- 在移动输入被阻断时保留方向意图

关键点：

- **输入意图和实际 AddMovementInput 被分开**
- 这让“角色暂时不能移动”和“玩家没有输入”不是一回事

### 4.2 基础移动层

主要类：

- `UCharacterMovementComponent`
- `UActCharacterMovementComponent`

职责：

- 基础跑动、跳跃、落地、碰撞、滑动
- 当前项目的自定义组件在原生基础上扩展 motion/rotation/network 逻辑

### 4.3 Motion 层

主要结构：

- `FActMotionParams`
- `FActMotionState`
- `MotionStateMap`

职责：

- 统一描述一段“战斗位移/旋转/launch/提取 root motion”
- 统一存储、更新、叠加、移除

它是本框架的核心。

### 4.4 RotationTask 层

主要能力：

- 锁敌转向
- 攻击转向
- 受击转向
- 方向转向
- 对活跃位移 basis 的附加/绝对修正

### 4.5 Ability Task 层

主要类：

- `UAT_ApplyAddMove`
- `UAT_MotionImpulse`
- `UAT_PlayMontageAndWaitForEvent`
- `UAT_PlayAddMoveMontageAndWaitForEvent`

职责：

- 把 gameplay/GA 层的意图包装成 Motion
- 把蒙太奇播放和 Motion 绑定起来

### 4.6 网络层

主要内容：

- 预测快照
- `SyncId`
- 服务端复制 `ReplicatedMotions`
- 客户端补追
- 特殊情况下抑制共享复制硬拉

---

## 5. 关键源码位置

| 模块 | 文件 |
| --- | --- |
| 角色移动组件 | `Source/ActGame/Public/Character/ActCharacterMovementComponent.h` |
| 角色移动实现 | `Source/ActGame/Private/Character/ActCharacterMovementComponent.cpp` |
| Motion 类型定义 | `Source/ActGame/Public/Character/ActCharacterMovementTypes.h` |
| 预测/网络结构 | `Source/ActGame/Public/Character/ActCharacterMovementNetworking.h` |
| 预测/网络实现 | `Source/ActGame/Private/Character/ActCharacterMovementNetworking.cpp` |
| 角色复制入口 | `Source/ActGame/Private/Character/ActCharacter.cpp` |
| 移动输入入口 | `Source/ActGame/Private/Character/ActHeroComponent.cpp` |
| 输入阻断 Notify | `Source/ActGame/Private/Character/ActCharacterNotifies.cpp` |
| 纯程序 AddMove Task | `Source/ActGame/Private/AbilitySystem/Abilities/Tasks/AT_ApplyAddMove.cpp` |
| 权威冲量/击飞 Task | `Source/ActGame/Private/AbilitySystem/Abilities/Tasks/AT_MotionImpulse.cpp` |
| 纯表现蒙太奇 Task | `Source/ActGame/Private/AbilitySystem/Abilities/Tasks/AT_PlayMontageAndWaitForEvent.cpp` |
| 提取 RootMotion 的蒙太奇 Task | `Source/ActGame/Private/AbilitySystem/Abilities/Tasks/AT_PlayAddMoveMontageAndWaitForEvent.cpp` |

---

## 6. 运动模型：框架是怎么抽象“位移”的

### 6.1 `FActMotionParams`

`FActMotionParams` 是整个 Motion 层的统一描述结构。

它描述一段 motion 的所有关键维度：

- 来源：`Velocity` 或 `MontageRootMotion`
- 参考系：`World` / `ActorStartFrozen` / `ActorLive` / `MeshStartFrozen` / `MeshLive`
- 平移速度：`Velocity`
- Launch：`LaunchVelocity`
- 生命周期：`Duration`
- 生效条件：`ModeFilter`
- 强度曲线：`CurveType` / `Curve`
- 可选旋转请求：`Rotation`
- 网络语义：`Provenance`
- 联机对齐 id：`SyncId`

### 6.2 `EActMotionBasisMode`

这个枚举决定“作者写的位移向量如何变成世界位移”。

| Basis | 含义 | 适合场景 |
| --- | --- | --- |
| `World` | 直接世界空间 | 固定方向击退、环境推力 |
| `ActorStartFrozen` | 以角色开始时朝向冻结 | 冲刺、攻击前冲、锁定起始朝向的位移 |
| `ActorLive` | 跟随角色当前朝向实时变化 | 少数需要实时跟随朝向的移动 |
| `MeshStartFrozen` | 以 Mesh 开始时朝向冻结 | RootMotion 提取、依赖动画朝向的位移 |
| `MeshLive` | 跟随 Mesh 当前朝向实时变化 | 少数贴合动画骨架朝向的位移 |

**为什么需要这层抽象：**

动作游戏很多位移不是简单“向前”。  
关键在于“向哪个前”，而且这个“前”有时必须冻结，有时必须实时追随。

### 6.3 `EActMotionProvenance`

这个枚举不是“表现类型”，而是**网络语义**。

| Provenance | 含义 | 典型用途 |
| --- | --- | --- |
| `OwnerPredicted` | 所有者预测 motion | 自己的攻击前冲、冲刺、技能位移 |
| `AuthorityExternal` | 服务器外力 | 别人把我击退/击飞 |
| `ReplicatedExternal` | 网络同步后的外部 motion | 客户端收到服务端复制后内部使用 |
| `LocalRuntime` | 纯本地运行时逻辑 | 锁敌转向这类不需要同步的本地辅助旋转 |

**为什么必须有这个区分：**

位移是否要预测、是否要复制、谁说了算，完全取决于 motion 的来源语义。  
动作游戏里“自己位移”和“别人把我打飞”不是一类网络问题。

### 6.4 `FActMotionRotationParams`

这套结构描述旋转目标，而不是旋转结果。

可表达：

- 朝某个方向
- 面向目标
- 背向目标
- 对齐目标 forward
- 使用 additive 或 absolute 方式影响已有位移 basis
- 旋转优先级

### 6.5 `FActMotionState`

这是运行时状态，不是策划配置。

它保存：

- Handle
- Params
- BasisMesh
- `ElapsedTime`
- `RotationElapsedTime`
- `StartLocation`
- `FrozenBasisRotation`
- `FrozenFacingDirection`
- 是否已完成旋转
- 是否被更高优先级暂停
- 是否压制了 RootMotion 自带旋转

换句话说：

- `Params` 决定“作者想要什么运动”
- `State` 决定“这段运动当前已经执行到哪里”

---

## 7. 基础 Move 执行链

### 7.1 目标

基础 Move 只解决正常跑动，不承载复杂战斗位移。

### 7.2 执行步骤

1. 输入进入 `UActHeroComponent::Input_Move`
2. 保存原始二维输入到 `MoveInputVector`
3. 把控制器朝向转换为世界移动方向
4. 如果当前没有被 `MovementInputBlock` 阻断，则调用 `Pawn->AddMovementInput`
5. 同时把角色相对方向推给指令分析器

### 7.3 为什么这样设计

这样设计的意义是：

- 基础移动继续复用 UE 成熟逻辑
- 角色不能移动时仍可保留输入方向
- 连招/指令系统仍能知道玩家当前想按哪个方向

这点非常重要。  
动作游戏中“不能移动”经常只意味着“当前帧不要把输入变成位移”，不意味着“把输入意图清空”。

### 7.4 输入阻断机制

`UANS_MovementInputBlock` 会在动画 NotifyState 生命周期内调用：

- `PushMovementInputBlock`
- `PopMovementInputBlock`

`UActHeroComponent` 内部会：

- 统计阻断源
- 清空 `PendingMovementInput`
- 下一次解除阻断时避免把阻断期间积累的移动脉冲一次性打出去

**为什么这样写：**

如果只是简单不调用 `AddMovementInput`，而不清理积累向量，角色容易在解锁后一瞬间突兀补一帧输入，手感会脏。

---

## 8. 状态参数层：为什么还有 `MovementStateParams`

### 8.1 作用

`FActMovementStateParams` 用于控制：

- `MaxWalkSpeed`
- `MaxAcceleration`
- `BrakingDecelerationWalking`
- `GroundFriction`
- `RotationRate`
- `bOrientRotationToMovement`
- `bUseControllerDesiredRotation`

### 8.2 提供的接口

- `ApplyMovementStateParams`
- `ResetMovementStateParams`
- `PushMovementStateParams`
- `PopMovementStateParams`
- `ClearMovementStateParamsStack`

### 8.3 为什么做成“栈”

动作游戏里速度状态往往是临时叠加的：

- 冲刺进入时改一套参数
- 技能蓄力再改一套参数
- 技能结束时恢复上一层

如果只维护一个“当前状态”，生命周期很容易乱。  
做成句柄栈有几个好处：

- 覆盖关系清晰
- 结束时能按句柄恢复
- 不同系统可以局部接管，不必互相知道彼此细节

### 8.4 当前项目接入情况

从当前源码检索看，组件已经提供了完整接口，但业务层暂未看到大量直接调用。  
这说明它是框架能力的一部分，适合后续 Sprint、蓄力、瞄准等状态扩展。

---

## 9. Motion 层核心执行链

### 9.1 创建 motion

所有 motion 最终都会进入：

- `ApplyMotion`
- `ApplyRotationMotion`
- `ApplyRootMotionMotion`
- 内部统一走 `ApplyMotionInternal`

### 9.2 `ApplyMotionInternal` 做了什么

它完成了 Motion 生命周期的初始化：

1. 校验 `Duration`
2. 必要时自动分配 `SyncId`
3. 获取或复用 `Handle`
4. 创建/更新 `FActMotionState`
5. 初始化 `ElapsedTime` / `RotationElapsedTime`
6. 记录 `ServerStartTimeSeconds`
7. 捕获冻结 basis
8. 必要时立即应用 launch
9. 更新 `MotionStateMapBySyncId`
10. 刷新预测 revision、网络修正模式、旋转任务和复制状态

### 9.3 为什么创建时就捕获 basis

因为很多战斗位移要求：

- 开始时朝向决定了整段位移
- 后续角色朝向变化不能影响这段位移

例如攻击前冲：

- 角色挥刀时锁定开招方向
- 后面镜头或角色旋转变化不应该把整段前冲轨迹拧弯

这就是 `ActorStartFrozen` / `MeshStartFrozen` 的意义。

---

## 10. 每帧位移消费链

### 10.1 总入口

`UActCharacterMovementComponent::OnMovementUpdated`

流程如下：

1. 先执行原生 `UCharacterMovementComponent::OnMovementUpdated`
2. 更新加速度与落地状态
3. 更新锁敌旋转任务
4. 刷新当前活跃旋转任务
5. 调用 `ApplyPendingMotion`

### 10.2 `ApplyPendingMotion`

这一步负责真正把 Motion 变成角色位移：

1. 调用 `ConsumeMotionDisplacement`
2. 得到本帧总平移位移
3. 尝试执行活跃旋转任务
4. 如果没有显式旋转任务，则看是否有 RootMotion 提供的旋转 delta
5. 最终通过 `SafeMoveUpdatedComponent` 执行位移

### 10.3 `ConsumeMotionDisplacement`

这是最关键的 runtime 逻辑。

它会遍历 `MotionStateMap`，对每个 active motion 执行：

1. 检查 `ModeFilter`
2. 判断是否已过期
3. 计算本帧有效 delta time
4. 调用 `ResolveMotionDisplacement`
5. 检查 NaN
6. 累加到总位移
7. 处理 RootMotion 旋转 delta
8. 更新 `ElapsedTime`
9. 结束时移除已到期条目

### 10.4 为什么不是直接改 `Velocity`

因为动作游戏里的战斗位移常常不是“真实物理速度”的同义词。

例如：

- 攻击前冲是作者指定的位移过程
- 技能后撤是一个有限时长的明确轨迹
- 击退可能只想覆盖一部分轴

如果全塞到 `Velocity`，会和基础移动、重力、制动、摩擦纠缠在一起，工程上不好控。  
所以这里选的是：

- 原生 `Velocity` 继续服务基础移动和物理
- 显式战斗位移额外叠加到最终位移结果

这就是文档里“基础移动 + 额外移动偏移 + 物理速度变化”那套思路。

---

## 11. 曲线、Duration 与 Launch

### 11.1 强度曲线

`EvaluateMotionScale` 支持：

- Constant
- EaseIn
- EaseOut
- EaseInOut
- CustomCurve

**为什么要做在 Motion 层，而不是只在动画里做：**

- 这样曲线参与的是 gameplay 位移本身
- 曲线结果可进入预测与同步
- 不依赖动画帧数

### 11.2 Launch

`LaunchVelocity` 是 motion 的另一种附带能力。

特点：

- 创建时只应用一次
- 可分别覆盖 XY / Z
- 会切换到 Falling
- 会根据竖直速度开启一段外力修正宽限期

### 11.3 为什么 Launch 不直接走 `LaunchCharacter`

因为它需要和 Motion 生命周期、basis、同步和预测绑定在一起。  
如果单独分出去，会破坏“一个技能动作的一整套运动语义”。

---

## 12. RotationTask：为什么要单独做旋转任务系统

### 12.1 要解决的问题

动作游戏里旋转来源很多：

- 基础移动朝向
- 锁敌朝向
- 攻击转身
- 受击朝向
- RootMotion 自带旋转

如果不仲裁，结果通常是：

- 这一帧锁敌要转
- 下一帧攻击也要转
- RootMotion 还想带 yaw
- 最后角色朝向抖、抢权或直接错

### 12.2 框架做法

所有旋转请求也进入 Motion 框架，由 `RefreshActiveRotationTask` 统一挑选一个**当前生效的旋转任务**。

规则：

- 先看 `CanStateDriveRotationTask`
- 按 `Priority` 选择最高优先级
- 同优先级下后加入者优先
- 非活跃旋转任务会被标记 `bRotationPaused`

### 12.3 `TickActiveRotationTask`

活跃旋转任务每帧会：

1. 解析当前目标朝向
2. 计算期望旋转
3. 按剩余时间插值旋转
4. 如果是 additive，则把旋转 delta 施加到所有冻结 basis motion
5. 如果是 absolute，则把这些 motion 的 basis 直接改成新朝向
6. 更新 `RotationElapsedTime`
7. 完成后标记结束，必要时移除

### 12.4 为什么旋转还能影响已有平移 basis

这是一个非常关键的设计。

如果攻击位移是 `ActorStartFrozen`：

- 位移方向在开始时被冻结
- 但某些旋转任务希望“带着这段位移一起转”

例如：

- 锁敌下的调整身位
- 攻击过程中向目标修正朝向

所以框架提供：

- `ApplyAdditiveRotationToActiveMotionBases`
- `ApplyAbsoluteRotationToActiveMotionBases`

不是只改角色面朝方向，而是连位移 basis 一起修。

### 12.5 锁敌旋转为什么是 `LocalRuntime`

`UpdateLockOnRotationTask` 里锁敌旋转是本地运行时逻辑，不走完整网络同步。

原因：

- 锁敌辅助更偏表现和操控体验
- 它依赖当前输入加速度和锁定目标
- 不值得把每一段锁敌调整都变成权威同步 motion

但它仍然进入统一 RotationTask 框架，这样不会与攻击/受击旋转逻辑割裂。

---

## 13. RootMotion 三种模式

### 13.1 `Procedural`

含义：

- 蒙太奇只负责表现
- gameplay 位移完全由程序 motion 负责

适合：

- 绝大多数攻击
- 技能前冲
- 受击表现

### 13.2 `ExtractedRootMotion`

含义：

- 蒙太奇确实包含作者做好的 root motion
- 但不让 UE 原生直接执行
- 先禁用 native root motion
- 再把指定 section/range 的 root motion 提取成 Motion

适合：

- 作者已经精细打磨过的位移动画
- 需要保留动画位移形状，但又要接入预测/同步体系

### 13.3 `NativeRootMotion`

含义：

- 保留原生 root motion 执行

适合：

- 极少数不需要预测、同步要求低、完全表现驱动的场景

### 13.4 为什么默认不推荐 NativeRootMotion

因为本项目的设计目标不是“方便”，而是“本地表现优先且联机稳定”。  
只要 gameplay 位移参与战斗判定、连招、受击、移动同步，就不应默认交给原生 root motion runtime。

---

## 14. `PlayMontageAndWaitForEvent` 执行链

### 14.1 定位

这是默认蒙太奇任务。

设计定位非常明确：

- 蒙太奇只是表现层
- gameplay 位移由别处负责

### 14.2 关键行为

- 播放蒙太奇
- 绑定事件和结束回调
- 默认把 `AnimRootMotionTranslationScale` 设为 `0`
- 结束时恢复为 `1`

### 14.3 为什么默认把 root motion 平移缩放设为 0

因为这是防止“作者动画里带了一点 root motion，结果 gameplay 流程被污染”的保险措施。  
对动作游戏来说，位移执行权必须清晰。

如果一个看起来只是纯表现的攻击蒙太奇偷偷挪了角色一点位置：

- 客户端和服务端可能挪得不一致
- 技能位移与蒙太奇位移会叠加失真
- 排查起来非常难

所以默认归零是正确的工程选择。

---

## 15. `PlayAddMoveMontageAndWaitForEvent` 执行链

### 15.1 适用场景

当动画里真的已经有作者想保留的位移轨迹时，用这个 task。

### 15.2 执行步骤

1. 播放蒙太奇
2. 如果允许本地应用 extracted add move：
   - 解析初始 section
   - 刷新该 section 的 extracted motion
3. `TickTask` 中，如果切换到新的 section，再次刷新 extracted motion
4. 每次刷新时：
   - 先 `PushDisableRootMotion`
   - 根据 section 或指定范围计算 track range
   - 计算对应 duration
   - 调用 `ApplyRootMotionMotion`

### 15.3 `ApplyRootMotionMotion` 怎么执行

它把：

- `Montage`
- `StartTrackPosition`
- `EndTrackPosition`
- `Duration`
- basis
- 是否应用 rotation

封装成一个 `MontageRootMotion` 类型的 motion。

之后每帧在 `ResolveMotionDisplacement` 中：

1. 按当前 elapsed alpha 计算 track 区间
2. 调用 `ExtractRootMotionFromTrackRange`
3. 把提取到的平移转成 basis 对应的世界位移
4. 可选输出 root motion rotation delta

### 15.4 为什么不是整段蒙太奇一次性转成位移

因为：

- 蒙太奇可能换 section
- section 可能决定连招阶段
- gameplay 需要按当前 section 动态刷新

当前实现按 section 刷新 extracted motion，是比较稳妥的工程折中。

---

## 16. `ApplyAddMove`：纯程序位移任务

### 16.1 适合场景

- 攻击前冲
- dash
- 程序定义的后撤
- 击退但不带复杂 launch
- 明确知道方向与强度的位移

### 16.2 执行逻辑

它会：

1. 归一化方向
2. `Velocity = Direction * Strength`
3. 选曲线类型
4. 需要时生成稳定 `SyncId`
5. 通过 `ApplyMotion` 创建一条 `Velocity` motion

### 16.3 为什么还要区分 `ApplyAddMove` 和 `ApplyAddMoveVelocity`

因为实际业务里两类 authoring 都存在：

- “给一个方向 + 强度”
- “我已经算好了最终 velocity”

框架同时支持这两种入口，减少上层重复计算。

---

## 17. `MotionImpulse`：为什么单独做权威冲量任务

### 17.1 适合场景

- 受击击退
- 受击击飞
- 服务器强制施加的外力位移

### 17.2 与 `ApplyAddMove` 的核心区别

`ApplyAddMove` 更偏“角色自己作者化位移”。  
`MotionImpulse` 更偏“服务器权威外力”。

它的特点：

- 只在 authority 上真正创建 motion
- 客户端上的表现性实例会静默 no-op
- `Provenance = AuthorityExternal`
- 会生成 `SyncId`
- 同时可带旋转请求

### 17.3 为什么受击位移必须偏服务端权威

因为被击退/击飞的目标不应该由受击者客户端自己决定。  
否则很容易出现：

- 受击客户端立刻把自己位置改回去
- 服务端认为应该被击飞
- 其他客户端看到的位移很小或错位

当前实现里还专门为竖直 launch 做了网络修正宽限期，这就是在解决“服务器施加外力后，远端客户端立刻把自己纠回去”的问题。

---

## 18. 网络同步：这是框架最值钱的部分

### 18.1 预测快照

本地预测 motion 会被保存为 `FActPredictedMotionSnapshot`，内容包括：

- `SyncId`
- `Params`
- `ElapsedTime`
- `RotationElapsedTime`
- 冻结 basis yaw
- 冻结 facing direction
- 旋转完成状态
- root motion rotation suppression 状态

### 18.2 为什么要把 snapshot 放进 SavedMove

因为普通 `CharacterMovement` 的 SavedMove 只知道：

- 位置
- 速度
- 加速度

但动作游戏的 battle motion 还需要知道：

- 当前是哪条 motion
- 跑到了哪一段
- basis 当时是什么
- 当前有没有被更高优先级旋转压制

否则回放预测时只还原位置是不够的。

### 18.3 客户端发送什么

客户端在 `SetMoveFor` 时捕获当前预测 motion 快照。  
在网络 move 序列化时，只发送有 `SyncId` 的 motion snapshot。

这说明设计上把 **可网络对齐的 motion** 和 **纯本地临时逻辑** 区分开了。

### 18.4 服务端复制什么

服务端不会只说“你现在在这个位置”。  
它还会复制 `ReplicatedMotions`，里面包含：

- `Params`
- `ServerStartTimeSeconds`
- `StartLocation`
- `FrozenBasisRotation`
- `FrozenFacingDirection`

也就是说，服务端复制的是：

**运动定义 + 运动起点 + 运动时间轴**

而不是只复制一个结果。

### 18.5 客户端收到复制后怎么处理

`SyncReplicatedMotions` 会：

1. 遍历收到的 replicated motion
2. 如果是本地已有的 owner-predicted motion，并且 sync id 对得上，则确认它
3. 否则创建/更新一条 `ReplicatedExternal` motion
4. 按服务端开始时间计算 `InitialElapsedTime`
5. 做一次 `ApplyReplicatedMotionCatchUp`
6. 删除本地多余的旧 motion

### 18.6 为什么要做 `ApplyReplicatedMotionCatchUp`

因为客户端收到服务端 motion 时，服务端那条 motion 已经不是 0 秒了。  
如果从头重新播，会显得慢半拍。  
所以要直接补到当前应该在的位置和旋转。

### 18.7 为什么还要“抑制共享复制硬拉”

项目里对 `AuthorityExternal` / `ReplicatedExternal` motion 还做了：

- `ShouldSuppressSharedReplication`
- `ApplySuppressedSharedReplication`

作用是：

- 当客户端本地轨迹和服务端轨迹已经很接近时
- 不要再走一次粗暴的共享复制位置硬拉
- 改用软修正保持接近

这对受击、击飞、外力运动观感很重要。

### 18.8 这套网络设计的直接收益

1. 本地攻击位移更稳
2. 受击击飞不会被受击者客户端轻易抵消
3. 模拟代理不会只看到“轻轻跳一下”
4. 回滚更像对齐同一条轨迹，而不是反复抽搐回拉

---

## 19. 为什么这套框架对弱网更友好

### 19.1 解决了什么

它主要解决的是：

- **带位移动作在弱网下被回滚拉位置**
- **RootMotion 直接作为 gameplay 位移执行权导致的进度与表现漂移**

### 19.2 还没有完全解决什么

如果连招窗口、取消窗口、派生窗口完全依赖动画 notify 与 montage 当前时间，弱网下仍会有问题。

原因是：

- 位置执行权被抽出来了
- 但如果“战斗阶段真相”仍然完全绑在动画实例进度上
- 那么弱网下 animation timeline 本身仍可能和权威状态不同步

### 19.3 当前项目的现状

项目里已经有 `AbilityChain` 体系，并通过 `AnimNotifyState` 打开/关闭 chain window。  
这说明项目已经意识到“单纯靠输入直通能力激活不够”，需要显式窗口系统。

但工程上应继续坚持一个原则：

**动画可以通知窗口何时打开，但战斗真相不应只依赖当前 montage 时间。**

如果后续继续强化弱网表现，应进一步把：

- 可连段窗口
- 可取消窗口
- 输入缓冲
- 权限校验

更多放到 Gameplay/AbilityChain 层，而不是完全交给动画 runtime。

---

## 20. 接口手册：`UActCharacterMovementComponent`

以下说明重点覆盖项目自定义接口。

### 20.1 基础查询

| 接口 | 用途 | 说明 |
| --- | --- | --- |
| `ResolveActMovementComponent` | 从 Avatar/Character 取自定义移动组件 | Ability Task 常用 |
| `GetGroundInfo` | 获取地面信息 | AnimInstance 等表现层读取 |
| `GetHasAcceleration` | 是否有加速度 | 常用于状态机或表现判断 |

### 20.2 状态参数接口

| 接口 | 用途 | 推荐场景 |
| --- | --- | --- |
| `ApplyMovementStateParams` | 直接设置一组状态参数 | 临时调试、一次性覆盖 |
| `ResetMovementStateParams` | 恢复默认状态参数 | 收尾 |
| `PushMovementStateParams` | 压栈覆盖状态参数 | Sprint、蓄力、瞄准、临时减速 |
| `PopMovementStateParams` | 弹出指定句柄 | 某状态结束 |
| `ClearMovementStateParamsStack` | 清空所有覆盖 | 强制恢复默认 |

**推荐：** gameplay 层优先使用 `Push/Pop`，少直接 `Apply`。

### 20.3 Motion 接口

| 接口 | 用途 | 推荐场景 |
| --- | --- | --- |
| `GenerateMotionSyncId` | 生成网络同步 id | 特殊自定义逻辑 |
| `ApplyMotion` | 通用 motion 入口 | 绝大多数自定义位移 |
| `ApplyRotationMotion` | 通用旋转入口 | 纯旋转需求 |
| `AddMoveRotationToTarget` | 朝目标旋转 | 攻击吸附朝向、面向锁定目标 |
| `AddMoveRotationToDirection` | 朝方向旋转 | 后撤转身、方向对齐 |
| `ApplyRootMotionMotion` | 提取 root motion 为 motion | 作者化 root motion 轨迹 |
| `StopMotion` | 按 handle 停止 | Task 销毁、技能中断 |
| `StopMotionBySyncId` | 按 sync id 停止 | 跨预测/复制对齐时收尾 |
| `StopAllMotion` | 停掉全部 motion | 死亡、强制重置 |
| `HasActiveMotion` | 是否有 active motion | 查询 |
| `HasMotionBySyncId` | 是否存在指定 sync id motion | 任务等待结束 |

### 20.4 网络相关接口

| 接口 | 用途 | 说明 |
| --- | --- | --- |
| `SyncReplicatedMotions` | 客户端同步服务端 motion | 由 `AActCharacter::OnRep_ReplicatedMotions` 调用 |
| `CapturePredictedMotionSnapshots` | 抓取本地预测快照 | SavedMove 发送前调用 |
| `RestorePredictedMotionSnapshots` | 回放预测快照 | 预测回放与 move 处理时调用 |
| `BuildReplicatedMotionEntries` | 服务端构建复制数据 | `AActCharacter` 调用 |
| `ShouldSuppressSharedReplication` | 判断是否抑制共享复制硬拉 | 外力 motion 观感优化 |
| `ApplySuppressedSharedReplication` | 应用软修正 | 模拟代理使用 |

### 20.5 一般使用原则

1. 业务层不要直接操作 `MotionStateMap`
2. gameplay 通过公开 API 或 AbilityTask 创建 motion
3. `Handle` 只用于本地生命周期管理
4. `SyncId` 用于跨预测/服务端/模拟端对齐

---

## 21. 接口手册：Ability Tasks

### 21.1 `UAT_PlayMontageAndWaitForEvent`

**定位：** 纯表现蒙太奇。

**适合：**

- 播放攻击动画
- 播放技能表现
- 通过 montage notify 发 gameplay event
- 位移由别的 task 负责

**不要用在：**

- 需要让原生 root motion 承担 gameplay 位移的默认攻击流程

### 21.2 `UAT_ApplyAddMove`

**定位：** 一段纯程序位移。

**适合：**

- 前冲
- dash
- 后撤
- 程序 authoring 的攻击推进

**典型参数建议：**

- 自身响应式动作用 `OwnerPredicted`
- 面向动画朝向的位移常用 `ActorStartFrozen` 或 `MeshStartFrozen`

### 21.3 `UAT_MotionImpulse`

**定位：** 一段服务端权威外力位移，可带旋转。

**适合：**

- 击退
- 击飞
- 受击反应

**注意：**

- 非 authority 上会 no-op
- 不要拿它做玩家自己输入触发的 dash

### 21.4 `UAT_PlayAddMoveMontageAndWaitForEvent`

**定位：** 蒙太奇带作者化 root motion，但执行权交给 Motion。

**适合：**

- 确实要保留动画轨迹形状
- 同时还要进入预测/同步体系

**不要默认使用：**

- 普通攻击只要程序位移能表达，就优先 `PlayMontageAndWaitForEvent + ApplyAddMove`

---

## 22. 推荐用法与工程范式

### 22.1 纯表现攻击动画

推荐组合：

- `UAT_PlayMontageAndWaitForEvent`
- 如需位移，再额外 `UAT_ApplyAddMove`

理由：

- 位移控制权清晰
- root motion 不会泄漏
- 易于预测和调参

### 22.2 玩家自己的 dash / 攻击前冲

推荐组合：

- `UAT_ApplyAddMove`
- `Provenance = OwnerPredicted`
- 合理使用 `BasisMode`

理由：

- 本地立刻响应
- 服务端可对齐同一条 motion

### 22.3 受击击退 / 击飞

推荐组合：

- `UAT_MotionImpulse`
- 由服务端施加

理由：

- 受击目标不应自己决定外力
- 可同时带 launch 和旋转

### 22.4 作者已经精细打磨过的位移动画

推荐组合：

- `UAT_PlayAddMoveMontageAndWaitForEvent`

理由：

- 保留动画轨迹
- 仍接入 motion 预测/同步

### 22.5 临时限制基础移动但保留输入意图

推荐方式：

- `UANS_MovementInputBlock`

理由：

- 角色此时不移动
- 但输入分析器仍可知道玩家方向

---

## 23. 反模式：不要这样用

### 23.1 不要默认让攻击蒙太奇直接驱动原生 RootMotion

这会重新引入：

- 位移回滚
- 位置与进度耦合
- 弱网下动作时序漂移

### 23.2 不要把连招窗口真相完全绑在 montage 当前时间

表现可以参考动画，  
但 gameplay 真相应尽量向 AbilityChain / gameplay timeline 收拢。

### 23.3 不要直接 `SetActorLocation` 实现战斗位移

这样会绕过：

- basis
- rotation task
- prediction
- replicated motion
- suppression correction

### 23.4 不要把服务器外力写成 `OwnerPredicted`

自己的 dash 与别人把我打飞，在网络语义上完全不是同一个问题。

### 23.5 不要滥用 `LocalRuntime`

`LocalRuntime` 适合锁敌这种本地辅助行为。  
带 gameplay 后果的 motion 不应该用它。

---

## 24. 工程收益总结

这套框架的收益可以归纳为 8 条。

1. **本地表现优先**
   - 玩家手感不会卡在服务器确认之后

2. **位移执行权清晰**
   - 蒙太奇负责表现，Motion 负责 gameplay 位移

3. **RootMotion 不再是黑盒**
   - 可以当数据来源，但不强绑执行权

4. **程序位移与动画位移统一管理**
   - 同一个框架处理 AddMove、Launch、Rotation、Extracted RootMotion

5. **旋转冲突可控**
   - 通过优先级和 pause 机制统一仲裁

6. **弱网观感更稳定**
   - 位置回滚不再直接等同于动作体验崩坏

7. **服务端外力更可信**
   - 击退/击飞不容易被客户端抵消

8. **后续扩展成本更低**
   - 新技能只需组合已有 motion 能力，而不是再造一套移动系统

---

## 25. 当前项目里的已知观察

### 25.1 已经明确成型的部分

- 基础 Move 与 Motion 分层已经成型
- RootMotion 三模式已经成型
- 网络预测/复制/补追已经成型
- RotationTask 仲裁已经成型
- 输入阻断但保留输入意图已经成型

### 25.2 当前源码里未看到明显接入的部分

- `MovementStateParams` 已有完整接口，但暂未检索到大量业务调用

### 25.3 与外部文档相比未看到的点

- 未看到类似 `PauseLocks` 的通用暂停机制
- 未在 C++ 源码中看到 `WalkRunMix` 这类动画回流移动强度系数

这不代表项目没有相关能力，  
只代表从当前 C++ 源码层面看，还没有明显对应实现。

---

## 26. 最后的设计原则

如果只记住一条原则，请记住这句：

**动作游戏里，动画可以提供位移数据，但 gameplay 位移执行权不能默认交给动画 runtime。**

当前项目这套移动框架的全部设计，最终都在服务这件事：

- 本地表现优先
- 服务器权威不丢
- 弱网回滚不破坏动作体验
- 蒙太奇是表现层，不是唯一的战斗真相

这也是为什么引擎已经有 `CharacterMovement`、`Montage`、`RootMotion`、`GAS`，项目仍然必须再做一层自己的移动框架。

因为动作游戏真正要解决的从来不是“能不能动”，而是：

**在复杂战斗、联机预测、弱网回滚、本地表现优先这些约束同时存在时，还能不能稳定地动。**

