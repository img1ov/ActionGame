# BulletSystem 模块设计详解

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
- `FBulletDataMove`：运动类型、速度、跟踪、固定时长、环绕等。
- `FBulletDataExecution`：LogicData 列表，驱动玩法逻辑扩展。

### 3.2 关键能力

- **配置拆分清晰**：按领域拆解，不混杂逻辑与表现。
- **简单子弹快速路径**：`CheckSimpleBullet()` 标记无逻辑/召唤/交互的子弹。
- **配置缓存**：`UBulletConfigSubsystem` 提供全局与 Owner/Class 级缓存，降低查表成本。
- **预加载**：对 LogicData/Niagara/Mesh 等资源做轻量预加载。

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
5. 预加载 Tick
6. AfterTick：系统收尾与销毁清理

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

通过 `DestroyBullet` Action 统一处理：

- 标记销毁、清理碰撞状态
- 触发 OnDestroy 逻辑
- 触发子弹生成（可选）
- 回收渲染/实体资源

避免直接销毁导致的资源泄露或顺序问题。

## 8. 高频系统层

### 8.1 MoveSystem

负责每帧位置更新：

- Straight / Parabola / Orbit / Follow / FixedDuration / Attached
- Homing 追踪
- 允许 `ReplaceMove` 逻辑替换默认运动

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

- 命中生成子弹（CreateBullet）
- 命中冻结（Freeze）
- 曲线替换运动（CurveMovement）
- 支援/护盾/反弹

## 10. 性能与复用

- `BulletPool`：实体池
- `BulletActorPool`：表现 Actor 池
- `BulletTraceElementPool`：碰撞 Trace 临时对象池
- `ConfigSubsystem`：配置缓存 + 预加载
- `BudgetComponent`：高频系统降采样更新

目标是控制 GC 抖动、减少创建销毁开销、稳定帧率。

## 11. 生命周期流程（简版）

1. CreateBullet
2. 查配置 → 创建 BulletInfo/Entity
3. InitBullet Action 入队
4. InitMove/InitCollision/InitRender 等子动作执行
5. MoveSystem / CollisionSystem 高频运行
6. 命中触发逻辑控制器
7. Destroy Action 统一清理
8. 回收对象池

## 12. 维护关注点

- 配置合法性：Shape/CollisionMode/HitTrigger 等组合必须正确。
- Action 顺序：新增 Action 必须保证与现有生命周期一致。
- LogicData 扩展：特殊玩法优先通过 Controller 扩展。
- 性能：高频路径必须避免动态分配与复杂分支。
