# EntityModule 教学文档

## 文档目的

这份文档用于教学说明 `Assets/GameScripts/HotFix/GameLogic/Module/EntityModule` 这一套实体模块，重点回答 4 个问题：

1. 这个模块到底是什么。
2. 它在项目里应该怎么用。
3. 它相比纯 `MonoBehaviour` 逻辑组织方式有什么好处。
4. 后续还有哪些值得继续优化的地方。

---

## 一、先说结论：这是一套“逻辑实体树 + 组件生命周期调度”框架

`EntityModule` 不是 Unity DOTS，也不是 Unity 原生 `GameObject + MonoBehaviour` 组件系统的替代品，而是项目在 HotFix 层自己封装的一套逻辑实体框架。

它的核心思想是：

- 用 `Entity` 表示逻辑实体。
- 用“子实体”组织层级关系。
- 用“逻辑组件”挂到实体上扩展能力。
- 用统一的生命周期接口驱动 `Awake / Update / FixedUpdate / LateUpdate / Destroy`。
- 用 `EntityScene` 作为逻辑场景根节点。
- 用依赖系统和异步实体系统补足复杂玩法所需能力。

可以把它理解成：

> 一套独立于 Unity MonoBehaviour 生命周期之外的、可控的游戏逻辑组织框架。

---

## 二、核心目录结构

`EntityModule` 目录下主要分为几块：

### 1. 模块入口

- `EntityModule.cs`

职责：

- 创建全局 `EntityRoot`
- 管理多个 `EntityScene`
- 驱动 `GameTimer`
- 初始化生命周期管理器、依赖管理器、ID 生成器

### 2. 核心实体

- `Core/Entity.cs`
- `Core/EntityScene.cs`
- `Core/EntityIdGenerator.cs`
- `Core/EntityRef.cs`
- `Core/EntityComponentView.cs`
- `Core/SingletonEntity.cs`

职责：

- 定义实体、子实体、组件、父子关系、销毁流程
- 生成实体 `Id / InstanceId`
- 提供安全引用包装
- 把逻辑实体映射到 Unity 层级中的 `GameObject`

### 3. 生命周期调度

- `Interfaces/IEntityLifecycle.cs`
- `ComponentSystem/ComponentLifecycleManager.cs`
- `ComponentSystem/ComponentManager.cs`
- `Core/GameTimer.cs`

职责：

- 定义生命周期接口
- 管理哪些组件参与 Update / FixedUpdate / LateUpdate
- 在每帧统一调度这些逻辑组件

### 4. 依赖系统

- `Dependency/EntityDependsOnAttribute.cs`
- `Dependency/IEntityDependentComponent.cs`
- `Dependency/EntityDependentComponentBase.cs`
- `Dependency/EntityDependencyRegistry.cs`
- `Dependency/EntityDependencyExtensions.cs`

职责：

- 声明“组件依赖哪些其他组件”
- 当依赖满足或失效时自动刷新组件激活状态

### 5. 异步实体

- `Async/AsyncEntity.cs`
- `Async/AsyncEntityExtensions.cs`

职责：

- 支持异步初始化实体/组件
- 适合资源加载、异步准备、延迟构建这类流程

---

## 三、整体运行链路

这套系统的主链路可以概括成下面 6 步：

### 第 1 步：拿到模块入口

项目里统一通过：

```csharp
var entityModule = GameModule.Entity;
```

`GameModule.Entity` 实际返回的是 `EntityModule.Instance`。

### 第 2 步：创建逻辑场景

`EntityModule` 用一个字典维护多个 `EntityScene`：

```csharp
EntityScene AddScene(string sceneName, EntityScene scene)
EntityScene GetScene(string sceneName)
void RemoveScene(string sceneName)
```

也就是说，真正的逻辑实体都应该挂在某个 `EntityScene` 下面。

### 第 3 步：在场景下创建实体树

所有实体都继承自 `Entity`，通过：

```csharp
scene.AddChild<T>()
entity.AddChild<T>()
```

来构建逻辑树。

### 第 4 步：给实体挂逻辑组件

一个实体还可以继续挂“逻辑组件”：

```csharp
entity.AddComponent<T>()
entity.GetComponent<T>()
entity.RemoveComponent<T>()
```

这里的组件不是 `MonoBehaviour`，而是同样继承 `Entity` 的逻辑对象。

### 第 5 步：统一驱动生命周期

如果某个实体/组件实现了这些接口：

- `IEntityAwake`
- `IEntityUpdate`
- `IEntityFixedUpdate`
- `IEntityLateUpdate`
- `IEntityDestroy`

系统会在合适时机自动调用它们。

### 第 6 步：销毁时递归清理

无论是 `scene.Dispose()`、`entity.Dispose()`、`RemoveScene()` 还是 `RemoveComponent<T>()`，最终都会走统一销毁流程，递归回收子实体、组件和生命周期注册。

---

## 四、核心对象怎么理解

## 4.1 Entity：逻辑世界里的基础节点

`Entity` 是整个系统的核心基类。

它负责管理：

- `Parent`
- `Children`
- `Components`
- `Id`
- `InstanceId`
- `IScene`
- `ViewGO`

可以把它理解成一个“既能当实体，又能当组件”的通用逻辑节点。

这也是这套设计的一个关键点：

> 子实体和组件本质上都是 `Entity`，只是挂载关系不同。

### 子实体和组件的区别

- 子实体：进入 `Children`
- 组件：进入 `Components`

两者都属于逻辑树的一部分，但语义不同：

- 子实体更偏“层级结构”
- 组件更偏“能力扩展”

---

## 4.2 EntityScene：逻辑场景根

`EntityScene` 继承自 `Entity`，同时实现了 `IEntityScene`。

它是整棵逻辑树的根节点，通常用来表示：

- 主城逻辑域
- 战斗逻辑域
- 副本逻辑域
- 测试逻辑域

项目里所有逻辑实体，理论上都应该归属于某个 `EntityScene`。

---

## 4.3 EntityModule：实体世界管理器

`EntityModule` 的职责不是直接管理每个实体，而是管理“实体世界运行环境”：

- 根节点 `EntityRoot`
- 所有逻辑场景 `_scenes`
- 全局时间驱动 `GameTimer`
- 生命周期系统和依赖系统的初始化/释放

可以理解为：

> `EntityModule` 管世界，`EntityScene` 管一个逻辑域，`Entity` 管具体对象。

---

## 4.4 ComponentLifecycleManager：生命周期调度中心

`ComponentLifecycleManager` 负责把实现了更新接口的实体注册进不同更新列表里：

- `IEntityUpdate`
- `IEntityFixedUpdate`
- `IEntityLateUpdate`

然后在 `GameTimer` 驱动下统一执行。

这样做的好处是：

- 生命周期入口统一
- 不依赖每个对象自己接 Unity `Update`
- 可控、可扩展、便于后续做统计和优化

---

## 五、这套模块怎么用

下面用一个简化示例说明标准使用方式。

## 5.1 第一步：定义一个场景类

```csharp
using GameLogic;

public class BattleScene : EntityScene
{
    public BattleScene(string name) : base(name)
    {
    }

    public override void Awake()
    {
        base.Awake();
    }
}
```

说明：

- `EntityScene` 是抽象类，需要先派生。
- 一般在构造函数里调用 `base(name)` 即可。

## 5.2 第二步：定义一个实体

```csharp
using GameLogic;

public class PlayerEntity : Entity, IEntityAwake
{
    public void Awake()
    {
    }
}
```

## 5.3 第三步：定义一个逻辑组件

```csharp
using GameLogic;

public class MoveComponent : Entity, IEntityAwake, IEntityUpdate
{
    public void Awake()
    {
    }

    public void Update(float deltaTime)
    {
    }
}
```

## 5.4 第四步：创建场景、实体和组件

```csharp
var scene = GameModule.Entity.AddScene("Battle", new BattleScene("Battle"));

var player = scene.AddChild<PlayerEntity>();
var moveComponent = player.AddComponent<MoveComponent>();
```

这段代码做了三件事：

1. 创建一个逻辑场景。
2. 在场景下创建一个玩家实体。
3. 给玩家挂上移动组件。

此后如果 `MoveComponent` 实现了 `IEntityUpdate`，就会自动进入统一更新。

## 5.5 第五步：查询与销毁

```csharp
var move = player.GetComponent<MoveComponent>();

player.RemoveComponent<MoveComponent>();

GameModule.Entity.RemoveScene("Battle");
```

销毁场景时，场景下的子实体和组件也会递归销毁。

---

## 六、进阶用法

## 6.1 组件依赖

如果一个组件依赖别的组件才能工作，可以用依赖系统。

### 写法 1：用特性声明依赖

```csharp
using GameLogic;

[EntityDependsOn(typeof(MoveComponent))]
public class MoveFxComponent : EntityDependentComponentBase, IEntityAwake, IEntityUpdate
{
    public void Awake()
    {
    }

    public void Update(float deltaTime)
    {
        if (!AreAllDependenciesMet)
            return;
    }

    protected override void OnActivationChanged(bool isActive)
    {
    }
}
```

### 写法 2：手动返回依赖列表

```csharp
public class SkillRuntimeComponent : EntityDependentComponentBase, IEntityAwake
{
    public void Awake()
    {
    }

    public override System.Type[] GetDependencyTypes()
    {
        return new[]
        {
            typeof(MoveComponent),
            typeof(AttributeComponent)
        };
    }
}
```

适合场景：

- 某组件必须依赖属性组件、移动组件、状态组件
- 组件加载顺序不固定
- 想避免空引用和顺序耦合

---

## 6.2 异步实体

如果某个组件需要异步加载资源、异步初始化，可以继承 `AsyncEntity`。

```csharp
using System.Threading;
using Cysharp.Threading.Tasks;
using GameLogic;

public class CharacterAvatarComponent : AsyncEntity, IEntityAwake
{
    public void Awake()
    {
    }

    protected override async UniTask OnLoadAsync(CancellationToken cancelToken)
    {
        await UniTask.Delay(100, cancellationToken: cancelToken);
    }
}
```

使用方式：

```csharp
var avatar = await player.AddComponentAsync<CharacterAvatarComponent>();
```

好处是：

- 异步初始化被纳入统一实体体系
- 取消或异常时，系统会自动移除半初始化对象
- 不容易留下脏状态

---

## 6.3 安全引用

`EntityRef<T>` 用 `InstanceId` 做了一层校验，适合保存实体引用，避免对象销毁后继续误用。

```csharp
EntityRef<PlayerEntity> playerRef = player;

PlayerEntity cachedPlayer = playerRef;
if (cachedPlayer != null)
{
}
```

---

## 七、这套模块的好处是什么

## 7.1 逻辑组织更清晰

相比把大量逻辑直接堆在 `MonoBehaviour` 上，这套系统把逻辑明确拆成了：

- 场景
- 实体
- 组件
- 生命周期
- 依赖
- 异步初始化

结构更清晰，复杂玩法更容易维护。

## 7.2 生命周期统一管理

所有逻辑更新不需要散落在各个 Unity 组件的 `Update` 里，而是收口到统一调度器中。

好处包括：

- 更容易控制执行顺序
- 更容易做性能分析
- 更容易做暂停、过滤、批量关闭

## 7.3 支持实体树表达复杂关系

像“角色 -> 武器 -> 技能表现节点”这种层级关系，用子实体表示会比一堆散乱脚本更直观。

## 7.4 组件能力组合更自然

一个实体可以只关心自己有哪些逻辑能力，例如：

- `MoveComponent`
- `AttributeComponent`
- `SkillComponent`
- `BuffComponent`

比把所有逻辑揉成一个超大脚本更利于拆分。

## 7.5 依赖系统能降低初始化顺序问题

组件不用强依赖“必须先添加谁再添加谁”，而是交给框架判断依赖是否满足。

## 7.6 异步初始化接入自然

资源加载型组件、延迟构建型组件可以直接使用 `AsyncEntity`，不会把异步流程散落到业务外面。

## 7.7 更适合 HotFix 层承载纯逻辑

这套模块放在 `Assets/GameScripts/HotFix/GameLogic` 下，本身就非常适合承接：

- 角色逻辑
- 战斗逻辑
- 关卡逻辑
- 状态机逻辑
- 技能逻辑

也就是“偏玩法、偏逻辑”的部分。

---

## 八、当前这个模块的现状判断

从当前仓库内调用痕迹来看，这套 `EntityModule` 已经具备完整基础能力，但业务主链路里还没有看到很多真实落地调用。

当前比较明确的事实有：

1. `GameModule` 已经暴露了 `GameModule.Entity` 入口。
2. `EntityModule` 目录本身已经具备场景、实体、依赖、异步、生命周期完整结构。
3. 仓库里没有搜到实际继承 `EntityScene` 的业务场景类。
4. 也没有搜到明显的 `GameModule.Entity.AddScene(...)` 业务接入代码。

这说明它目前更像：

> 一套已经搭好的逻辑实体基础设施，但项目主业务暂时还没有全面切进来。

这并不是坏事，反而说明当前正处于“适合逐步接入和教学推广”的阶段。

---

## 九、使用这套模块时要注意什么

## 9.1 它不是 MonoBehaviour

`Entity` 和组件都不是 Unity 原生组件，不会自动收到 Unity 生命周期函数。

要参与生命周期，必须实现对应接口：

- `IEntityAwake`
- `IEntityUpdate`
- `IEntityFixedUpdate`
- `IEntityLateUpdate`
- `IEntityDestroy`

## 9.2 同类型组件默认只能挂一个

`Components` 使用类型哈希做 key，所以一个实体默认只能有一个同类型组件。

这适合大多数玩法组件，但不适合同类型多实例组件场景。

## 9.3 逻辑实体默认会生成 ViewGO

实体被注册到场景后，会自动创建一个 `ViewGO`，并挂到 `EntityRoot` 或父实体节点下。

这对调试很方便，但实体数量很大时需要关注对象开销。

## 9.4 场景接入方式需要先约定好

在真正落地之前，最好先明确：

- 哪些业务用 `EntityModule`
- 一个 Unity Scene 是否对应一个 `EntityScene`
- 场景切换时谁负责 `AddScene / RemoveScene`

---

## 十、后续可以怎么优化

## 10.1 先补“默认接入入口”

当前最值得优先补的是一个清晰的接入流程，例如：

- 游戏启动时创建默认逻辑场景
- 场景切换时自动增删 `EntityScene`
- 给业务提供统一创建实体的入口

这样团队成员才能真正知道这套框架应该从哪里开始用。

## 10.2 区分“纯逻辑实体”和“需要视图的实体”

目前实体注册后默认会创建 `ViewGO`。后续可以考虑：

- 纯逻辑实体不创建 `ViewGO`
- 按需延迟创建 `ViewGO`
- 只给调试模式创建 `ViewGO`

这样可以减少层级污染和对象开销。

## 10.3 做反射缓存

当前框架里有一些反射型操作，例如：

- `Activator.CreateInstance`
- `MakeGenericType`
- `GetCustomAttributes`

后续如果实体规模上来，建议缓存：

- 生命周期接口信息
- 依赖特性信息
- `ComponentManager<T>` 构造结果

## 10.4 增强调试与可视化能力

建议后续补这些能力：

- 当前场景实体数
- 当前组件数
- 每帧更新耗时
- 依赖未满足的组件列表
- 实体树调试窗口

这样更利于教学、排错和性能优化。

## 10.5 明确与现有 MonoBehaviour 体系的边界

当前项目里还有一套现成的 `Player / Character / Component` 逻辑在走 `MonoBehaviour` 风格。

后续最好明确 3 种策略里的哪一种：

1. `EntityModule` 只负责纯逻辑层，表现层仍然是 `MonoBehaviour`
2. 新功能逐步迁移到 `EntityModule`
3. 两套系统长期并存，但职责边界要非常清楚

如果边界不清晰，后面很容易出现“双套实体模型并行维护”的问题。

## 10.6 增加示例场景和教学样例

当前最适合教学推进的方式，不是先大规模改业务，而是先补：

- 一个 `TestScene : EntityScene`
- 一个 `PlayerEntity`
- 一个 `MoveComponent`
- 一个依赖组件示例
- 一个异步组件示例

让团队先按例子理解，再逐步迁移真实业务。

---

## 十一、教学时建议怎么讲

如果你要拿这套模块做团队教学，建议按这个顺序讲：

1. 先讲“为什么不用所有逻辑都直接写 MonoBehaviour”。
2. 再讲 `EntityModule -> EntityScene -> Entity -> Component` 这条主链路。
3. 再演示最小示例：创建场景、创建实体、挂组件、自动 Update。
4. 再讲依赖系统解决什么问题。
5. 最后讲异步实体和未来优化方向。

这样更符合学习路径，也更容易让大家理解这套系统的价值。

---

## 十二、总结

`EntityModule` 本质上是一套面向 HotFix 逻辑层的实体框架，它把“实体层级、组件能力、生命周期调度、依赖关系、异步初始化”放进了同一套模型里。

它的最大价值，不是单纯替代 `MonoBehaviour`，而是让复杂玩法逻辑拥有更清晰、更可控、更可演进的组织方式。

如果后续把默认接入流程、调试能力、示例项目和与现有业务体系的边界再补齐，这套模块会非常适合作为项目里的通用逻辑实体框架。
