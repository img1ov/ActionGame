﻿# BulletSystem 使用教程

本文是面向项目落地的使用说明，按 "把系统跑起来" 的顺序写。实现细节与架构分层请看 `BulletSystem_Design.md`。

## 1. 你会用到的几个概念

- `BulletId`：配置表里的一行子弹配置的主键。
- `UBulletConfig`：配置资产，聚合若干张 DataTable 和少量 Inline 行，运行时提供 `BulletId -> FBulletDataMain` 查表能力。
- `FBulletInitParams`：一次 Spawn 的输入参数。当前约定：`SpawnTransform` 必须由调用方提供，系统不做 Owner 兜底。
- `InstanceId`：运行时子弹实例 id，用于后续操作（手动结算/销毁/开关碰撞等）。
- `InstanceKey`：运行时句柄（名字）。用于在 AnimNotify/蓝图里跨时机找回 `InstanceId`，本质是 `Key -> InstanceId` 的本地映射表。
- `Payload`：每次 Spawn 携带的 SetByCaller 数值，用于命中时注入到 GE（或你自己的逻辑里读取）。

## 1.1 HitReact（最稳：GameplayEvent 驱动 GA）

如果你希望做“类 DMC5 的受击表现”（击退/击飞/硬直/浮空等），推荐让子弹命中后在服务器侧：

1. 先 Apply 伤害/状态 GE（数值与规则）
2. 再对目标 ASC 发送 `GameplayEvent`（动作与表现），由 `GA_HitReact` 使用 `ActivateAbilityFromEvent` 接收 `EventData`

BulletSystem 已在 `UBulletLogicData_ApplyGameplayEffect` 提供可选开关：

- `bApplyHitReact`

`HitReactImpulse` 的来源：

- 推荐：在 `UAN_SpawnBullet` / `UANS_SpawnBullet` 的 `HitReactImpulse` 字段里配置（spawn 前注入到 `InitParams.Payload`）
- 或者：在你的 GA 收到 SpawnEvent 后，修改 `OptionalObject->InitParams.Payload.HitReactImpulse` 再 spawn（同样会随子弹实例携带到命中逻辑）

运行时规则：当 GE 成功应用到目标后，如果 `InitParams.Payload.HitReactImpulse.HitReactTag` 有效，则会调用 `TargetASC->HandleGameplayEvent(HitReactTag, EventData)`：

- `EventData.EventTag`：`HitReactImpulse.HitReactTag`
- `EventData.EventMagnitude`：`HitReactImpulse.Strength`
- `EventData.TargetData`：`FHitReactImpulseTargetData`（GA 蓝图里用 `UBulletSystemBlueprintLibrary::GetHitReactImpulseFromEventData` 读取）
- `EventData.ContextHandle`：同一份 `EffectContextHandle`（包含 HitResult/Instigator/Causer/SourceObject 等）

## 2. 配置准备（DataTable + UBulletConfig）

1. 创建一张 DataTable，RowStruct 选择 `FBulletDataMain`。
2. 给每行配置一个唯一的 `BulletId`。
3. 创建一个 `UBulletConfig` DataAsset，把 DataTable 填到 `BulletDataTables`（后面的表会覆盖前面的同名 BulletId）。
4. 可选：少量临时/测试配置可以放进 `InlineBulletData`，同样按 `BulletId` 覆盖。

配置侧最常用的两类：

- Projectile（扫掠命中，每帧检测）：`Base.CollisionMode=Sweep`，`Base.HitTrigger=EachFrame`。
- HitBox（重叠采集，手动结算）：`Base.CollisionMode=Overlap`，`Base.HitTrigger=Manual`，攻击帧再调用 `ProcessManualHits`。

## 3. 角色接入（BulletSystemComponent）

BulletSystem 的仿真是 World 级别的（`UBulletWorldSubsystem/UBulletController`），但你需要在角色/武器等 Actor 上挂一个 `UBulletSystemComponent` 来提供：

- 注入 `UBulletConfig`
- 维护 `InstanceKey -> InstanceId` 的本地映射（便于蓝图/AnimNotify 使用）
- 提供本地 Spawn/Destroy/ManualHit 的转发入口

最小接入：

1. 在你的 Character/Pawn（或武器 Actor）上添加 `UBulletSystemComponent`。
2. 在合适的时机调用 `SetBulletConfig(你的UBulletConfig资产)`。

## 4. 直接在蓝图 Spawn（最短可跑链路）

在蓝图中：

1. 构建一个 `FBulletInitParams`：
   - `Owner`：通常是当前角色
   - `SpawnTransform`：必须设置（例如取武器 socket 的世界 Transform）
   - 可选：`TargetActor/TargetLocation`（给 Aim/Homing/Follow 等用）
   - 可选：`Payload`（SetByCaller 数值）
   - 可选：`InstanceKey`（需要后续按名字找回 InstanceId 才填）
2. 调用 `UBulletSystemBlueprintLibrary::SpawnBullet(SourceActor, BulletId, InitParams)`，返回 `InstanceId`。

如果你打算后续在另外的时机结算/销毁，不想自己保存 `InstanceId`，就给 `InitParams.InstanceKey` 赋值，并在需要时调用：

- `UBulletSystemBlueprintLibrary::GetInstanceIdByKey(SourceActor, InstanceKey)`

注意：同一个 `InstanceKey` 在重叠窗口下可能会同时存在多个 Instance（例如连段/缓冲）。

- `GetInstanceIdByKey`：返回“最近一次 Spawn 且仍有效”的 `InstanceId`
- 原生 Destroy Notify：会按“最早 Spawn”的顺序依次 Destroy（避免重叠窗口误删新实例）

## 5. Manual HitBox（重叠采集 + 攻击帧结算）

当 `Base.HitTrigger=Manual` 时，CollisionSystem 不会为该 bullet 做每帧碰撞查询；只有当你在攻击帧调用 `ProcessManualHits` 时，系统才会做一次碰撞查询并派发 OnHit 逻辑链。

你需要在攻击帧调用：

- `UBulletSystemBlueprintLibrary::ProcessManualHits(WorldContext, InstanceId, bApplyCollisionResponse)`

推荐参数：

- `bApplyCollisionResponse=true`：命中后仍然执行配置的碰撞响应（Destroy/Pierce/Bounce/Support 等）。

## 6. 销毁语义（统一：帧末回收）

销毁只有两种来源：

- 显式调用 Destroy（例如 NotifyEnd、技能结束）
- LifeTime 到期（系统 Tick 到期后走同一路径）

两者都会走 `RequestDestroyBullet(InstanceId)` 标记销毁，真实回收在帧末 `FlushDestroyedBullets()` 统一完成，避免在高频迭代中直接移除导致不稳定。

## 7. 特效是否跟随子弹

跟随。

- `InitRender` 会为子弹拿到一个 `ABulletActor`，并把 Niagara/Mesh 组件挂在该 Actor 的 `RootComponent` 上。
- `MoveSystem` 每帧将 Actor 的位置旋转同步到 `BulletInfo.MoveInfo`。

因此特效天然跟随子弹运动（包括 `MoveType=Attached` 的跟随）。

注意：DedicatedServer 不会创建/更新渲染 Actor（只跑逻辑）。

## 8. AnimNotify / AnimNotifyState 的用法（推荐）

插件提供 4 个原生 Notify：

- `UAN_SpawnBullet`：在某一帧 Spawn
- `UAN_ProcessManualHits`：在某一帧手动结算
- `UAN_DestroyBullet`：在某一帧销毁
- `UANS_SpawnBullet`：Begin Spawn，End Destroy（窗口型）

它们都有 BlueprintNativeEvent 回调：

- 蓝图不覆写：走 C++ 默认逻辑（够用，配置式）
- 蓝图覆写且不 Call Parent：完全由蓝图自己控制（更灵活）

常用做法：

1. Spawn Notify 上配置 `BulletId`、`SpawnSocketName`、`InstanceKey`（建议显式填）。
2. HitBox 的攻击帧放 `UAN_ProcessManualHits`，使用同一个 `InstanceKey`。
3. 窗口结束用 `UAN_DestroyBullet` 或 `UANS_SpawnBullet` 的 End 自动 Destroy。

## 9. GAS 推荐链路（只讲最短必需点）

联机下推荐让 Authority 的真实 Spawn 发生在 GA 中：

1. `UAN_SpawnBullet` 在 Authority/Autonomous 侧发 GameplayEvent 给 GA（由 `SpawnEventTag` 路由）。
2. GA 收到 EventData 后，从 `EventData.OptionalObject` 读取 notify 参数（`UBulletNotifyOptionalObject`），并把伤害等 SetByCaller 写进 `InitParams.Payload`。
3. GA 调用 `SpawnBullet` 生成权威子弹。

模拟端（SimulatedProxy）不会跑 GA，但仍会触发 AnimNotify。此时 Notify 会本地 Spawn 用于表现（可开碰撞驱动本地命中特效），但不要在客户端做权威结算。

## 10. 调试与排查

- 日志：把 `LogBullet` 提到 `Verbose`。
- Manual HitBox 没伤害：先确认 `Base.HitTrigger=Manual`，再确认攻击帧有没有调用 `ProcessManualHits`。
- 同一个窗口找不到 Instance：确认 `InitParams.InstanceKey` 与后续查询用的是同一个 Key。
- PIE 修改配置不生效：可调用 `UBulletSystemBlueprintLibrary::ClearBulletSystemRuntime(WorldContext, true)` 清掉世界内缓存并重建运行时表。

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

# BulletSystem 模块设计详解

另见：`BulletSystem_Usage.md`（面向落地的使用教程）

> 面向实现与维护的工程说明，聚焦架构分层、运行流程、扩展点与性能策略。

## 1. 设计目标

- **配置驱动**：用数据描述子弹形态与行为组合，代码负责执行与调度。
- **生命周期可编排**：初始化、销毁、延迟等阶段显式分解，顺序可控。
- **高频系统解耦**：移动与碰撞高频更新独立实现，便于性能优化。
- **玩法逻辑可插拔**：用 LogicData + Controller 扩展个性玩法，不污染底层系统。
- **高性能复用**：对象池与配置缓存减少创建/销毁与资源抖动。

## 2. 总体架构（分层）

```text
配置/数据表
  ↓
BulletConfig / BulletDataMain
  ↓
BulletController（统一调度）
  ↓
BulletModel（运行态总仓库）
  ↓
BulletEntity + BulletInfo
  ↓
Action 生命周期编排      System 高频更新
  ↓                      ↓
Init/Destroy/Summon...   MoveSystem / CollisionSystem
  ↓
LogicController / Render / Interact / Pool
```

## 3. 配置层

### 3.1 核心数据结构

- `FBulletDataMain`：子弹配置入口，包含 Base/Logic/Move/Render/Execution/Child/Interact 等子块。
- `FBulletDataBase`：形状、生命周期、碰撞策略、命中间隔、碰撞模式等基础字段。
- `FBulletDataMove`：运动类型、速度、跟踪、固定时长、环绕等。出生基准永远来自 `InitParams.SpawnTransform`，配置侧只提供 Offset 等“模板级”修饰量。
- `FBulletDataExecution`：LogicData 列表，驱动玩法逻辑扩展。
- `FBulletDataRender`：仅负责表现资源（Niagara/Mesh/ActorClass）。表现资源挂在 `ABulletActor` 上，Actor 每帧由 MoveSystem 同步到 `MoveInfo`，因此特效会天然跟随子弹一起运动。跟随/附着由 `Move.MoveType=Attached` 统一负责，避免“表现跟随但碰撞不跟随”的无效选项。

### 3.2 关键能力

- **配置拆分清晰**：按领域拆解，不混杂逻辑与表现。
- **简单子弹快速路径**：`CheckSimpleBullet()` 标记无逻辑/召唤/交互的子弹。
- **配置缓存**：`UBulletConfig` 自身维护 BulletId -> Data 的运行时表缓存（懒构建/可重建），由 `BulletSystemComponent` 注入到运行时。
- **可选预加载**：资源预热由外部加载策略负责（子弹系统只消费已注入配置，避免引擎全局缓存耦合）。

## 4. 控制层（BulletController）

### 4.1 职责

- 初始化系统与池（Model/Pool/Action/System）。
- 统一创建与销毁入口，保证生命周期顺序。
- 驱动 Tick/AfterTick：先 System，再 Action。

### 4.2 Tick 顺序

1. 暂停 ActionRunner
2. `MoveSystem.OnTick()`
3. `CollisionSystem.OnTick()`
4. 恢复 ActionRunner，执行生命周期 Action
5. AfterTick：系统收尾与销毁清理

该顺序保证高频逻辑与生命周期编排互不干扰。

## 5. 运行态数据层

### 5.1 BulletModel

- 维护 `BulletMap`：所有存活子弹。
- 维护 `NeedDestroyBullets`：帧末统一销毁。
- 维护父子弹关系与攻击者索引。

### 5.2 BulletInfo

单颗子弹的运行态快照：

- InitParams / Config
- MoveInfo / CollisionInfo / EffectInfo / ChildInfo
- 状态字段：IsInit、NeedDestroy、LiveTime、Frozen、Tags
- Action 队列：`ActionInfoList` / `NextActionInfoList`

逻辑状态集中于 Info，渲染交给 Actor，降低引擎对象耦合。

## 6. 实体层

`UBulletEntity` 作为运行实体壳：

- 统一挂载 `BulletActionLogicComponent / BulletActorComponent / BulletBudgetComponent`。
- 可扩展自定义组件（高级轨迹/碰撞）。
- 对外抽象为逻辑实体，不承担表现。

## 7. 生命周期编排（Action 系统）

### 7.1 Action 设计

- `ActionInfo`：描述“要做什么”。
- `Action`：执行具体逻辑。
- `ActionCenter`：Action 工厂与池管理。
- `ActionRunner`：执行队列 + 持续 Action Tick。

### 7.2 初始化流水线

`InitBullet` 会派发：

- InitHit / InitMove / InitCollision / InitRender
- SummonEntity（可选）
- UpdateAttackerFrozen / UpdateEffect / SceneInteract
- UpdateLiveTime / TimeScale / AfterInit
- Child（如需子弹）

这样保证初始化阶段可扩展且顺序固定。

### 7.3 销毁流程

通过 `UBulletController::RequestDestroyBullet(InstanceId)` 统一处理（立即生效，帧末统一回收）：

- 标记销毁、清理碰撞状态
- 触发 OnDestroy 逻辑
- 触发子弹生成（由子弹配置 `bSpawnOnDestroy` 决定）
- 回收渲染/实体资源

避免直接销毁导致的资源泄露或顺序问题。

补充说明：

- 不区分销毁原因：主动删除和生命周期到期都走同一路径。
- `RequestDestroyBullet` 会立即触发 OnDestroy 并关闭碰撞，但对象池回收仍然在帧末 `FlushDestroyedBullets()` 统一完成（避免在高频系统迭代中直接移除导致不稳定）。

## 8. 高频系统层

### 8.1 MoveSystem

负责每帧位置更新：

- Straight / Parabola / Orbit / Follow / FixedDuration / Attached
- Homing 追踪
- 允许 `ReplaceMove` 逻辑替换默认运动

其中 `Attached` 会跟随 Owner 的 Transform，并保留进入 Attached 状态时捕获的相对偏移。

补充约定：

- `InitParams.SpawnTransform` 必须由调用方提供（系统不做 Owner 兜底）。
- `Move.SpawnLocationOffset` 解释为 **SpawnTransform 空间**下的偏移（跟是否 Attached 无冲突；Attached 时等价于改变初始相对偏移）。
- Aim 只影响初始方向解析（以及相关的旋转），不会改变出生基准来源。

设计理由（为什么不做兜底/不提供额外开关）：

- **语义单一**：Spawn 位置与朝向只由 `InitParams.SpawnTransform (+ Move 的 Offset)` 决定，避免“同一颗子弹到底用谁的 transform”这种隐式分支。
- **更适合 AN/ANS/GA 分工**：AN/ANS/GA 在调用点显式构建 InitParams，比在 DataMain 里用开关做推断更可控，也更容易查错。

### 8.2 CollisionSystem

负责碰撞检测与命中处理：

- Sweep / Overlap 模式
- Ray/Sphere/Box/Capsule 多形状
- HitInterval、CollisionStartDelay
- 自动/手动命中触发

Move 与 Collision 拆分，保证职责单一与性能可控。

## 9. 逻辑扩展层（LogicData + Controller）

- `Execution.LogicDataList` 描述逻辑模块。
- `BulletActionLogicComponent` 将 LogicData 转为 Controller 并按 Trigger 分组。
- 支持触发点：OnBegin、OnHit、OnDestroy、ReplaceMove、Tick 等。
- 支持 Blueprint 扩展。

示例扩展：

- 命中生成子弹（SummonBullet / SpawnBullet）
- 命中冻结（Freeze）
- 曲线替换运动（CurveMovement）
- 支援/护盾/反弹

## 10. 性能与复用

- `BulletPool`：实体池
- `BulletActorPool`：表现 Actor 池
- `BulletTraceElementPool`：碰撞 Trace 临时对象池
- `BulletConfig`：配置资产（运行时表缓存）
- `BulletSystemComponent`：配置注入 + 最小网络 Spawn 分发
- `BudgetComponent`：高频系统降采样更新

目标是控制 GC 抖动、减少创建销毁开销、稳定帧率。

### 10.1 PIE/StopPlay 清理策略（重要）

在 Editor 下，StopPlay 后再次 Play 可能会复用同一进程中的缓存对象。如果不显式清理，可能出现：

- 修改了 DataTable/Config，但下次 Play 仍命中旧缓存
- 对象池复用到“上一次 Play 的子弹实体/表现 Actor”，造成行为与配置不一致

因此 BulletSystem 在 **World 维度** 做兜底清理，并提供 ProjectSettings 开关控制时机：

- ProjectSettings: `BulletSystemSettings.RuntimeResetPolicy`
  - `BeginPlayOnly` / `StopPlayOnly` / `BeginPlayAndStopPlay` / `None`
  - 默认：`BeginPlayAndStopPlay`
- 实现位置：`UBulletWorldSubsystem`
  - BeginPlay: `OnWorldBeginPlay()` 按策略调用 `UBulletController::Shutdown()` 清空对象池与存活子弹
  - StopPlay: 监听 `FGameDelegates::EndPlayMapDelegate`（以及 WorldCleanup 兜底）按策略重置
- 迭代/调试时可手动触发：
  - BP: `UBulletSystemBlueprintLibrary::ClearBulletSystemRuntime(WorldContext, bRebuildRuntimeTable)`

## 11. 生命周期流程（简版）

1. SpawnBullet
2. 查配置 → 创建 BulletInfo/Entity
3. InitBullet Action 入队
4. InitMove/InitCollision/InitRender 等子动作执行
5. MoveSystem / CollisionSystem 高频运行
6. 命中触发逻辑控制器
7. `RequestDestroyBullet` 标记销毁（立即关闭碰撞并触发 OnDestroy，回收在帧末 Flush）
8. 帧末 `FlushDestroyedBullets()` 回收对象池

## 12. 维护关注点

- 配置合法性：Shape/CollisionMode/HitTrigger 等组合必须正确。
- Action 顺序：新增 Action 必须保证与现有生命周期一致。
- LogicData 扩展：特殊玩法优先通过 Controller 扩展。
- 性能：高频路径必须避免动态分配与复杂分支。

## 13. 关键链路（详细）

### 13.1 创建子弹链路（从调用到入模）

1. Gameplay 侧调用
   - C++: `UActAbilitySystemComponent::SpawnBullet()`
   - BP: `UBulletSystemBlueprintLibrary::SpawnBullet(SourceActor, BulletId, InitParams)`
2. `UBulletSystemBlueprintLibrary` 通过 `IBulletSystemInterface` 找到 `UBulletSystemComponent` 并转发 `SpawnBullet()`
3. `UBulletSystemComponent` 调用 `UBulletWorldSubsystem -> UBulletController::SpawnBullet(..., ConfigAsset)` 创建实例
4. Controller 解析配置
   - `UBulletConfig::GetBulletData(BulletId, OutData)`
5. Model 创建运行态
   - `UBulletModel::SpawnBullet()` 生成 `InstanceId`
   - 从 `UBulletPool` 复用/创建 `UBulletEntity`
   - 写入 `FBulletInfo.InitParams / Config / Size / Parent` 等
     - 其中 `InitParams.Payload` 可携带“每次发射的实例数据”（常用于给可复用 GE 注入 SetByCaller 伤害）
6. 入队生命周期 Action
   - `EnqueueAction(InstanceId, InitBullet)`

### 13.2 InitBullet 初始化流水线（动作链）

`InitBullet` 会做：

- 计算并缓存 `bIsSimple = Config.CheckSimpleBullet()`
- 初始化 Budget 组件（高频降采样）
- 合并 Tags（Config.Base.Tags + Config.Logic.LogicTags + runtime tags）
- 如果不是 Simple 且有 LogicComponent：
  - `LogicComponent.Initialize(Execution.LogicDataList)`
- 入队后续动作（顺序固定）
  - `InitHit -> InitMove -> InitCollision -> InitRender -> ... -> AfterInit`

### 13.3 Tick 顺序（Subsystem -> Controller -> System/Action）

`UBulletWorldSubsystem::Tick()`：

1. `Controller.OnTick(DeltaSeconds)`
2. `Controller.OnAfterTick(DeltaSeconds)`

Controller 内部顺序（重要）：

1. Pause ActionRunner（避免 Action 队列迭代时被修改）
2. `MoveSystem.OnTick()`
3. `CollisionSystem.OnTick()`
4. Resume ActionRunner，执行动作队列（Init/Destroy/Delay 等）
5. AfterTick：FlushDestroyedBullets（回收 Actor/Entity，移除 Model 条目）

### 13.4 Children（父子弹 / 子子弹）

BulletSystem 的 Child 机制用于“子弹事件驱动的派生子弹”，典型用途包括分裂弹、弹片、命中扩散、死亡爆炸等。

#### 13.4.1 配置入口（DataMain.Children）

每条 `FBulletDataChild` 代表一种派生规则，常用字段语义如下：

- `ChildBulletId`：要生成的子弹配置 ID
- `Count`：生成数量
- `bSpawnOnHit / bSpawnOnDestroy`：触发时机（命中 / 销毁）；都为 false 时等价于 “OnCreate”
- `SpawnLocationOffset`：生成偏移
- `bSpawnLocationOffsetInSpawnSpace`：偏移是否在 spawn transform 空间计算（否则按世界空间）
- `SpawnRotationOffset`：生成旋转偏移
- `bInheritOwner / bInheritTarget / bInheritPayload`：是否继承 Owner/Target/Payload（继承到 Child 的 `FBulletInitParams`）
- 注意：如果 Parent 是通过 `OverrideConfig`（武器/技能专用配置资产）生成的，Child 会默认使用同一个配置资产来解析 `ChildBulletId`，避免因未注入配置导致“找不到子弹行”。

#### 13.4.2 触发链路（OnCreate / OnHit / OnDestroy）

- **OnCreate**：当既不 OnHit 也不 OnDestroy 时，系统在创建后通过 Action 链路生成子弹（保持生命周期顺序）。
- **OnHit**：`HandleHitResult(...)` 内部会调用 `RequestSummonChildren(Info, OnHit)`，再把 spawn 转为 `SummonBullet` Action 入队，避免 tick 中途改写运行态容器。
- **OnDestroy**：`RequestDestroyBullet(...)` 会触发 `RequestSummonChildren(Info, OnDestroy)`，派生子弹仍通过 `SummonBullet` Action 入队，避免 tick 中途改写运行态容器。

#### 13.4.3 继承与生命周期绑定（ParentInstanceId / Parent 清理）

- Spawn 子弹时会通过 `BuildChildParams(...)` 把 `ParentInstanceId` 等写入 Child 的 `FBulletInitParams`，并按 `bInheritOwner / bInheritTarget / bInheritPayload` 决定是否继承对应字段。
- 当 Parent 被销毁时，系统会在 `FlushDestroyedBullets()` 阶段清理“已经存在的”子弹（会跳过那些在 parent destroy 时刻之后才生成的 child），保证父子弹生命周期关系可控，避免出现“父弹死了但早先生成的子弹永远不清理”的悬挂状态。

## 14. 碰撞与命中（最容易踩坑的部分）

BulletSystem 的碰撞不是基于 `UBoxComponent/USphereComponent`，而是每 tick 做 scene query：

- `CollisionMode = Sweep`：`SweepMultiByChannel(...)`
- `CollisionMode = Overlap`：`OverlapMultiByChannel(...)`
- `Shape = Ray`：`LineTraceMultiByChannel(...)`
- `Shape = Sphere/Box/Capsule`：使用对应的 `FCollisionShape::Make*`

### 14.1 EachFrame vs Manual（为什么你看到红框但 OnHit 不触发）

`Base.HitTrigger` 决定是否自动派发命中逻辑：

- `HitTrigger = EachFrame`
  - CollisionSystem 在检测到命中后会调用 `Controller->HandleHitResult(...)`
  - 进而触发 Logic 链：`LogicController.OnHit(...)`

- `HitTrigger = Manual`
  - CollisionSystem 不会做每帧碰撞查询
  - 因此不会自动派发 `HandleHitResult` / `OnHit`
  - 正确用法：在你需要结算的时机调用 `UBulletSystemBlueprintLibrary::ProcessManualHits(WorldContext, InstanceId, bApplyCollisionResponse)`

这也是 HitBox profile 的默认设计：HitBox = Overlap + Manual，适合“攻击帧/节奏点”手动结算。

### 14.2 HitInterval（重复命中间隔）

- `Base.HitInterval <= 0`：不做间隔限制，允许每次都命中。
- `Base.HitInterval > 0`：同一个 Actor 两次命中之间必须间隔 >= HitInterval。

注意：HitInterval 只对“同一个 Actor 的重复命中”生效，不影响第一次命中。

### 14.3 CollisionStartDelay（开火保护期）

`Base.CollisionStartDelay` 用于避免 muzzle 出口/贴脸时立刻碰撞：

- 未到时间：CollisionSystem 直接跳过该 bullet 的碰撞查询，并清空 OverlapActors。

### 14.4 场景交互（Interact / SceneInteract）

Interact 是“命中某些场景 Actor 时触发额外交互逻辑”的扩展点，典型用于可破坏物、机关、场景触发器等。

#### 14.4.1 配置入口

`FBulletDataMain.Interact`（`FBulletDataInteract`）当前只有两个开关：

- `bEnableInteract`：是否启用交互链路
- `bAffectEnvironment`：是否允许影响环境（当前实现里它也会作为触发 `OnBulletInteract` 的门槛）

#### 14.4.2 启用链路（Action 阶段）

当 `Interact.bEnableInteract` 或 `Obstacle.bEnableObstacle` 为 true 时，`InitBullet` 会入队 `SceneInteract` Action。

`SceneInteract` Action 目前做的事情是：

- 给子弹打上 tag：`Bullet.Interact`（如果该 tag 在项目 GameplayTags 里存在）

这个 tag 是预留扩展点，便于后续在逻辑/筛选/统计里识别“具备交互意图”的子弹。

#### 14.4.3 触发点（命中阶段）

实际触发发生在 `UBulletController::HandleHitResult(...)`：

- 条件：`Interact.bEnableInteract && Interact.bAffectEnvironment && HitActor != null`
- 如果 `HitActor` 实现了 `UBulletInteractInterface`
  - 调用 `OnBulletInteract(BulletInfo, Hit)`

注意：该回调发生在碰撞响应（Destroy/Bounce/Pierce/Support）之前，也发生在 `RequestSummonChildren(OnHit)` 之前。

#### 14.4.4 预留扩展点

- 目标 Actor 侧扩展：实现 `UBulletInteractInterface`（C++ 或蓝图）即可接入交互回调
- Bullet 侧扩展：
  - 继续扩展 `FBulletDataInteract`（例如：InteractTags、只对特定 ActorClass 生效、触发次数限制、触发时机 OnHit/OnDestroy 等）
  - 或新增独立 Action/LogicController，把交互从 `HandleHitResult` 中拆出来（更易测试与组合）

## 15. 逻辑扩展（LogicData + Controller）

### 15.1 初始化条件（为什么有些逻辑永远不触发）

逻辑触发依赖两件事：

1. 配置层必须让 bullet 不是 simple：
   - `Execution.LogicDataList` 非空，或存在 Children/Summon/Interact/Obstacle 等
2. `LogicData` 能被加载且 `ControllerClass` 正确：
   - 软引用路径无效、资产未 cook、ControllerClass 未设置都会导致控制器未创建

建议打开日志：

- `LogBullet=Verbose` 或 `VeryVerbose`

### 15.2 Trigger 语义

- `OnBegin`：AfterInit 阶段调用（更接近“子弹开始工作”）
- `OnHit`：只有 EachFrame hit 才会自动触发；Manual hit 需要通过 ProcessManualHits 才会触发
- `ReplaceMove`：可替换默认移动（返回 true 则跳过默认 move）
- `Tick`：跟随 MoveSystem 的节奏更新（受 TimeScale/Budget 影响）

### 15.3 蓝图扩展（Blueprint Controller 实操）

BulletSystem 对蓝图开放的推荐扩展基类是：

- `UBulletLogicControllerBlueprintBase`

它会在 C++ 侧收到触发后，转发到蓝图可覆写的 `K2_*` 函数。

#### 15.3.1 最小接入步骤

1. 新建一个蓝图类，父类选择 `UBulletLogicControllerBlueprintBase`
2. 在蓝图里覆写你需要的事件/函数
   - `Event OnBegin`
   - `Event OnHit`
   - `Event OnDestroy`
   - `Event Tick`
   - `Override ReplaceMove` (返回 bool)
   - `Override FilterHit` (返回 bool)
3. 新建一个 `UBulletLogicData`（`PrimaryDataAsset`）
   - `Trigger = OnHit/OnBegin/...`
   - `ControllerClass = 你刚才的蓝图 Controller`
4. 在子弹配置（DataTable 行或配置资产）里，把该 LogicData 加到：
   - `Execution.LogicDataList`

只要 `Execution.LogicDataList` 非空，该子弹就不会走 simple fast-path，逻辑就会被初始化并参与触发。

#### 15.3.2 事件参数的注意事项（by-ref 陷阱）

- `OnBegin/OnHit/OnDestroy/Tick/...` 这些 **事件** 的 `BulletInfo` 以 `const FBulletInfo&` 形式传入。
  - 这是刻意设计：蓝图 Event 无法“按引用回写”参数，否则会出现类似 “No value will be returned by reference” 的警告。
  - 结论：不要试图在 `Event OnHit` 里修改 `BulletInfo` 来影响后续系统。

- 需要“修改子弹运行态”的场景，请用 **可返回值的 override**：
  - `ReplaceMove(ref BulletInfo, DeltaSeconds) -> bool`
    - 返回 `true` 表示你完全接管移动，MoveSystem 将跳过默认移动
    - 你可以修改 `BulletInfo.MoveInfo.Location/Velocity/...` 来驱动轨迹
  - `FilterHit(ref BulletInfo, Hit) -> bool`
    - 返回 `false` 则本次命中会被忽略（不会进入 HandleHitResult / 不会触发 OnHit 链）

#### 15.3.3 OnHit 触发的前提（EachFrame vs Manual）

你在场景里看到调试框变红，只说明“查询命中”，不代表会触发 `OnHit`：

- `Base.HitTrigger = EachFrame`
  - 系统会对每个命中调用 `HandleHitResult`，从而触发 `OnHit`
- `Base.HitTrigger = Manual`
  - CollisionSystem 不会做每帧碰撞查询，因此不会自动触发 `OnHit`
  - 你需要在合适的时机调用 `ProcessManualHits`，它会做一次碰撞查询并逐个派发命中处理

HitBox profile 默认就是 Manual，适合“攻击帧结算”。

#### 15.3.4 常见蓝图“没触发”的排查顺序

1. 子弹行里 `Execution.LogicDataList` 是否真的填了该 LogicData
2. LogicData 的 `ControllerClass` 是否设置，且能被运行时加载（软引用路径有效）
3. `Trigger` 是否选对（比如你写了 OnHit 但 Trigger 还是 OnBegin）
4. `HitTrigger` 是否为 Manual（Manual 下 OnHit 不会自动触发）
5. `CollisionStartDelay` 是否导致前几帧碰撞被跳过
6. 是否被 `FilterHit` 过滤掉（返回 false）

建议把 `LogBullet` 调到 `Verbose` 或 `VeryVerbose`，并关注 `BulletLogic:` 前缀的日志（LogicData 加载失败时会有提示）。

#### 15.3.5 蓝图里应用 GE/伤害（推荐姿势）

在 `Event OnHit` 里做 GE 通常比在 Execution 里“反查子弹”更稳定：

- Source（伤害来源）：`BulletInfo.InitParams.Owner`
- Target（受击目标）：`Hit.GetActor()`（为空时再 fallback 到 `BulletInfo.InitParams.TargetActor`）
- 伤害数值：用 `SetByCaller` 或 GE 自己捕获属性

注意网络侧：如果你的子弹只在 Server 创建，那么蓝图 OnHit 的打印/Apply 也只会发生在 Server（客户端只会看到表现，不会有逻辑回调）。

#### 15.3.5.1 Payload 驱动的 SetByCaller（GA 伤害注入）

当你想让 GA 计算伤害，但由子弹的 Hit 负责“命中时刻的结算与 ApplyGE”，推荐用 Payload（类似 `GameplayEffectSpec` 的 per-shot 参数包）携带每次发射的动态数值：

- GA/Gameplay 在 `SpawnBullet` 时写入：
  - `InitParams.Payload.SetByCallerNameMagnitudes` 或
  - `InitParams.Payload.SetByCallerTagMagnitudes`
  - 蓝图侧推荐用 `UBulletSystemBlueprintLibrary::SetPayloadSetByCallerMagnitudeByName/Tag` 写入（避免直接暴露 payload map）
- 子弹配置里添加 `UBulletLogicData_ApplyGameplayEffect`（命中触发）
- 命中时 `UBulletLogicController_ApplyGameplayEffect` 会创建 `GameplayEffectSpec`，把 Payload 的 SetByCaller 写入后再 Apply 到目标

注意：Payload 在 `SpawnBullet` 时会被拷贝进运行态 `BulletInfo.InitParams`，Spawn 之后再改你手里的 `InitParams` 不会影响已经生成的子弹；运行时通常只读取 Payload（用于 GE 的 SetByCaller 注入）。

子弹分裂/派生时的继承规则：

- Child 默认会继承 Parent 的 Payload（用于“弹片同源伤害”等场景）
- 如需阻断继承，在 `DataMain.Children` 对应条目里把 `bInheritPayload = false`

设计约束：

- BulletSystem 核心（Controller/Model/System/Action）不依赖 GAS。
- GAS 相关实现只存在于 `UBulletLogicController_ApplyGameplayEffect` 的实现文件 `Plugins/BulletSystem/Source/Private/Logic/BulletLogicControllerTypes.cpp` 中；仅该 `.cpp` 会包含 GameplayAbilities 相关头文件，避免扩散到其它子弹代码路径。

#### 15.3.6 Manual HitBox 的“攻击帧结算”模板

当 `Base.HitTrigger = Manual` 时，HitBox 只在“攻击帧结算”时做一次碰撞查询。你需要在攻击帧调用：

- `UBulletSystemBlueprintLibrary::ProcessManualHits(WorldContext, InstanceId, bApplyCollisionResponse)`

它会做一次碰撞查询，并把命中的 Actor 逐个走 `HandleHitResult`，从而触发 `OnHit` 逻辑链。

#### 15.3.7 蓝图可用 API 速查（BulletSystemBlueprintLibrary）

- `SpawnBullet(SourceActor, BulletId, InitParams) -> InstanceId`
- `GetInstanceIdByKey(SourceActor, InstanceKey) -> InstanceId`（InstanceRegistry 查询）
- `ConsumeOldestInstanceIdByKey(SourceActor, InstanceKey) -> InstanceId`（InstanceRegistry FIFO 消费）
- `DestroyBullet(WorldContext, InstanceId)`
- `IsBulletValid(WorldContext, InstanceId)`
- `SetBulletCollisionEnabled(WorldContext, InstanceId, bEnabled, bClearOverlaps)`
- `ProcessManualHits(WorldContext, InstanceId, bApplyCollisionResponse) -> AppliedCount`
- `SetInitParamsSetByCallerMagnitudeByName(InitParams, DataName, Magnitude)`（推荐，带执行引脚）
- `SetInitParamsSetByCallerMagnitudeByTag(InitParams, DataTag, Magnitude)`（推荐，带执行引脚）
- `SetPayloadSetByCallerMagnitudeByName(InitParams.Payload, DataName, Magnitude)`（可链式写法，注意它会修改入参）
- `SetPayloadSetByCallerMagnitudeByTag(InitParams.Payload, DataTag, Magnitude)`（可链式写法，注意它会修改入参）
- `ClearInitParamsPayload(InitParams)` / `ClearPayload(Payload)`
- `GetPayloadSetByCallerMagnitudeByName(BulletInfo, DataName) -> (bFound, Magnitude)`
- `GetPayloadSetByCallerMagnitudeByTag(BulletInfo, DataTag) -> (bFound, Magnitude)`
- `GetPayloadSetByCallerMagnitudeByNameFromPayload(Payload, DataName) -> (bFound, Magnitude)`
- `GetPayloadSetByCallerMagnitudeByTagFromPayload(Payload, DataTag) -> (bFound, Magnitude)`

#### 15.3.8 蓝图使用指南（最短链路）

- **纯蓝图/非 GAS**：AnimNotify(或任意 BP) 构建 `FBulletInitParams`（Owner/SpawnTransform/Payload/InstanceKey），直接调用 `SpawnBullet`。需要手动结算的 HitBox 则在攻击帧调用 `ProcessManualHits`。
- **GAS（推荐）**：AN/ANS 只负责“时机”，GA 构建/补全 `InitParams.Payload`（SetByCaller），最终调用 `SpawnBullet`。伤害结算在 Server 的 OnHit 里 Apply GE（Payload 注入 SetByCaller）。
- **InstanceKey**：当你需要“Spawn 后在其它时机 ProcessManualHits/Destroy”时，用 `InitParams.InstanceKey` 做运行态句柄；通过 `GetInstanceIdByKey` 找到 `InstanceId` 后再调用对应函数。

## 16. 配置案例

### 16.1 典型 Projectile（扫掠命中，每帧检测）

- `ConfigProfile = Projectile`
- `Base.Shape = Sphere`
- `Base.CollisionMode = Sweep`
- `Base.HitTrigger = EachFrame`
- `Base.CollisionChannel = WorldDynamic`（按需）
- `Base.bDestroyOnHit = true`
- `Base.MaxHitCount = 1`

执行链路：Spawn -> Init -> MoveTick -> SweepHit -> HandleHitResult -> OnHit -> Destroy

### 16.2 典型 HitBox（重叠采集，手动结算）

- `ConfigProfile = HitBox`
- `Base.Shape = Box`
- `Base.CollisionMode = Overlap`
- `Base.HitTrigger = Manual`
- `Base.CollisionChannel = Pawn`（按需）
- `Base.bDestroyOnHit = false`
- `Base.MaxHitCount = 999`

执行链路：Spawn -> Init -> 你在攻击帧调用 ProcessManualHits(内部做一次碰撞查询) -> HandleHitResult -> OnHit

### 16.3 射线武器（Ray）

- `Base.Shape = Ray`
- `Base.CollisionMode = Sweep`（对 Ray 等价于 LineTrace）
- `Base.HitTrigger = EachFrame` 或 `Manual`

注意：Ray 模式下 `FBulletRayInfo.TraceStart/TraceEnd` 会记录最近一次 trace。

## 17. 测试与调试清单

### 17.1 最小化验证

1. 开启 debug draw（如果 Controller 支持）
2. 将 `LogBullet=Verbose`
3. 用一个固定目标（放一个 Box/StaticMeshActor）验证：
   - Sweep 模式：是否产生 Hits
   - Overlap 模式：是否产生 Overlaps
4. 验证 `HitTrigger`：
   - EachFrame：是否触发 OnHit
   - Manual：是否需要手动 ProcessManualHits 才触发 OnHit

### 17.2 常见问题定位

- 红框但没有 OnHit：检查 `Base.HitTrigger` 是否为 Manual
- OnHit 没触发且 bullet 似乎是 simple：检查 `Execution.LogicDataList` 是否真的写到了该 bullet 行
- LogicData 不生效：检查软引用资产是否可加载、`ControllerClass` 是否设置、是否被 cook

---

## 18. 联机与 AnimNotify 集成（BulletSystem）

本插件的 BulletSystem 核心（Controller/Model/Action/System）不依赖网络复制；联机下推荐采用如下分工：

- **服务端（Authority）**：运行子弹完整逻辑链路，命中时通过 GAS 结算伤害（GE），并以 GE/Cue 负责“权威结果”的同步。
- **自主代理（AutonomousProxy）**：和服务端一致跑 GA（LocalPredicted / Server），在 GA 中注入 Payload（SetByCaller 等）后再 Spawn 子弹。
- **模拟端（SimulatedProxy）**：不执行 GA，但仍会播放蒙太奇并触发 AnimNotify。模拟端应 **直接本地 Spawn 子弹** 用于表现，且建议开启碰撞用于驱动击中特效等视觉反馈；但伤害/环境交互必须保持权威（服务端执行）。

### 18.1 InstanceKey 运行时句柄

AnimNotify 的 ProcessManualHits/Destroy 需要 `InstanceId`。在联机下，`InstanceId` 无法从 Notify 直接稳定获取，因此 BulletSystem 引入了 **InstanceKey**：

- `AN_SpawnBullet`：写入 `InitParams.InstanceKey`（推荐显式配置）。如果未配置，则默认用 `BulletId` 作为 Key。Spawn 后由 `UBulletSystemComponent` 的 `FBulletInstanceRegistry` 将 `Key -> InstanceId` 写入运行时表。
- `AN_ProcessManualHits` / `AN_DestroyBullet`：用同一个 `InstanceKey` 解析到 `InstanceId`，再调用 `ProcessManualHits/DestroyBullet`。

蓝图查询入口：

- `UBulletSystemBlueprintLibrary::GetInstanceIdByKey(SourceActor, InstanceKey)`（内部转发到 `UBulletSystemComponent::GetInstanceIdByKey`）
- `UBulletSystemBlueprintLibrary::ConsumeOldestInstanceIdByKey(SourceActor, InstanceKey)`（消费最早 Spawn 的实例，适合 End/Destroy）

### 18.2 ANS_SpawnBullet（窗口型）

`ANS_SpawnBullet` 用 Begin/End 两个时机提供一个“Spawn 窗口”：

- Begin：执行 Spawn，并把 `Key -> InstanceId` 写入运行时表（`Key` 为空则默认用 `BulletId`）
- End：用同一个 Key 消费（Consume）“最早 Spawn 且仍有效”的 `InstanceId` 并执行 Destroy

如果存在多个窗口可能重叠，建议显式配置不同的 `InstanceKey`，避免 `ProcessManualHits` 指向歧义（默认 Key 查询会返回“最新 Spawn”的实例）。

注意：Authority/Autonomous 侧如果配置了 `EndEventTag`，End 阶段会通过 GameplayEvent 通知 GA 执行 Destroy；
未配置时将直接按 `InstanceKey` 在本地 Destroy（适合非 GAS 场景）。

### 18.3 权威守门（环境交互）

`Interact.bAffectEnvironment` 代表会影响环境/世界状态的交互回调。为了避免客户端双跑，插件侧已在命中处理里加了守门：客户端跳过该回调，仅服务端执行。
