# BulletSystem 使用教程

本文是面向项目落地的使用说明，按 "把系统跑起来" 的顺序写。实现细节与架构分层请看 `BulletSystem_Design.md`。

## 1. 你会用到的几个概念

- `BulletId`：配置表里的一行子弹配置的主键。
- `UBulletConfig`：配置资产，聚合若干张 DataTable 和少量 Inline 行，运行时提供 `BulletId -> FBulletDataMain` 查表能力。
- `FBulletInitParams`：一次 Spawn 的输入参数。当前约定：`SpawnTransform` 必须由调用方提供，系统不做 Owner 兜底。
- `InstanceId`：运行时子弹实例 id，用于后续操作（手动结算/销毁/开关碰撞等）。
- `InstanceKey`：运行时句柄（名字）。用于在 AnimNotify/蓝图里跨时机找回 `InstanceId`，本质是 `Key -> InstanceId` 的本地映射表。
- `Payload`：每次 Spawn 携带的 SetByCaller 数值，用于命中时注入到 GE（或你自己的逻辑里读取）。

## 2. 配置准备（DataTable + UBulletConfig）

1. 创建一张 DataTable，RowStruct 选择 `FBulletDataMain`。
2. 给每行配置一个唯一的 `BulletId`。
3. 创建一个 `UBulletConfig` DataAsset，把 DataTable 填到 `BulletDataTables`（后面的表会覆盖前面的同名 BulletId）。
4. 可选：少量临时/测试配置可以放进 `InlineBulletData`，同样按 `BulletId` 覆盖。

配置侧最常用的两类：

- Projectile（扫掠命中，自动触发）：`Base.CollisionMode=Sweep`，`Base.HitTrigger=Auto`。
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

注意：同一个 `InstanceKey` 会被覆盖为“最近一次 spawn 的那颗子弹”。重叠窗口一定要用不同 Key。

## 5. Manual HitBox（重叠采集 + 攻击帧结算）

当 `Base.HitTrigger=Manual` 时，碰撞系统只负责收集 `OverlapActors`，不会自动触发 OnHit。

你需要在攻击帧调用：

- `UBulletSystemBlueprintLibrary::ProcessManualHits(WorldContext, InstanceId, bResetHitActorsBefore, bApplyCollisionResponse)`

推荐参数：

- `bResetHitActorsBefore=true`：每次攻击帧允许重新命中同一批目标（做多段伤害/连段窗口时常用）。
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

