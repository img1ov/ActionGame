# BulletSystem 案例配置与GA使用

## 目标
覆盖两类需求：
1. 近战 HitBox 子弹（Overlap 收集，关键帧多次伤害）
2. 飞行弹道子弹（自动碰撞/自动触发）

本案例基于当前插件新增的配置项：
- `Base.CollisionMode`（`Sweep`/`Overlap`）
- `Base.HitTrigger`（`Auto`/`Manual`）
- `Base.bCollisionEnabledOnSpawn`
- `Move.SpawnOffset` + `Move.bSpawnOffsetInOwnerSpace`

---

## 案例 1：近战 HitBox（Overlap + 关键帧多次伤害）

### 配置建议（BulletDataMain）
- **Base**
  - `Shape = Box`
  - `BoxExtent = (60, 40, 60)`  // 视攻击范围调整
  - `CollisionMode = Overlap`
  - `HitTrigger = Manual`       // 不自动伤害，交给关键帧触发
  - `bCollisionEnabledOnSpawn = false`
  - `bDestroyOnHit = false`
  - `MaxHitCount = 999`         // 或足够大
  - `CollisionChannel = Pawn`   // 只扫目标通道
- **Move**
  - `MoveType = Attached`       // 跟随角色
  - `bUseSpawnTransform = false`
  - `bUseOwnerForward = true`
  - `SpawnOffset = (100, 0, 0)` // 角色前方偏移
  - `bSpawnOffsetInOwnerSpace = true`
- **Render**
  - 可不挂任何特效（纯碰撞盒）
- **Execution**
  - 绑定伤害/命中逻辑（在 `OnHit` 触发）

### GA 蒙太奇关键帧流程（示例）
1. **Spawn**（攻击开始 Notify）
   - `CreateBullet` -> 保存 `BulletId`
2. **HitWindowBegin**（判定开始）
   - `SetBulletCollisionEnabled(BulletId, true, true, true)`
3. **DamageFrame**（每个伤害关键帧）
   - `ApplyDamageToOverlaps(BulletId, true, false)`
     - `true`：每帧前清空命中名单，允许重复伤害
     - `false`：不走 Destroy/Bounce 等碰撞响应
4. **HitWindowEnd**（判定结束）
   - `SetBulletCollisionEnabled(BulletId, false, true, false)`
5. **Montage End**
   - `DestroyBullet(BulletId, External, false)` 回收

> 说明：
> - Overlap 目标列表会在碰撞开启期间实时更新，目标离开自动移除。
> - 多次伤害通过关键帧主动触发（不依赖自动碰撞）。

---

## 案例 2：飞行弹道子弹（Sweep + 自动命中）

### 配置建议（BulletDataMain）
- **Base**
  - `Shape = Sphere`
  - `SphereRadius = 10`
  - `CollisionMode = Sweep`
  - `HitTrigger = Auto`     // 让系统自动触发 OnHit
  - `bCollisionEnabledOnSpawn = true`
  - `bDestroyOnHit = true`
  - `MaxHitCount = 1`
  - `CollisionChannel = Pawn`
  - `CollisionResponse = Destroy`  // 或 Bounce/Pierce
- **Move**
  - `MoveType = Straight`
  - `Speed = 2000`
  - `bHoming = true` / `HomingAcceleration = 8000`（可选）
  - `bUseOwnerForward = true`
  - `SpawnOffset = (50, 0, 0)`

### GA 流程（示例）
1. **Spawn**（攻击开始 Notify）
   - `CreateBullet`
2. **自动命中**
   - Move/Collision 系统每帧处理
   - `OnHit` 触发逻辑伤害
   - `bDestroyOnHit = true` 自动回收

> 如需“穿透/多段命中”，可把 `bDestroyOnHit = false`，调高 `MaxHitCount`，或改为 `CollisionResponse = Pierce`。

---

## 备注
- `ApplyDamageToOverlaps` 支持 `bApplyCollisionResponse=true`，可在需要时让手动伤害也触发 Destroy/Bounce。
- 如果需要“命中间隔/重复命中节流”，后续可以在 Base 里扩展 `HitInterval` 并在 `ApplyDamageToOverlaps` 中判断。
