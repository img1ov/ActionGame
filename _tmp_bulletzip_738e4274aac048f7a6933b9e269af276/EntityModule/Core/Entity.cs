using System;
using System.Collections.Generic;
using TEngine;
using UnityEngine;

namespace GameLogic
{
    [Flags]
    internal enum EntityStatus : byte
    {
        None = 0,
        IsFromPool = 1,
        IsRegister = 1 << 1,
        IsComponent = 1 << 2,
        IsCreated = 1 << 3,
        IsNew = 1 << 4,
    }

    public class Entity : IDisposable
    {
        public GameObject ViewGO;

        public long InstanceId { get; protected set; }
        public long Id { get; protected internal set; }

        private EntityStatus _status = EntityStatus.None;
        private Entity _parent;
        internal SortedDictionary<long, Entity> _children;
        internal protected SortedDictionary<long, Entity> _components;
        protected IEntityScene _iScene;

        public bool IsFromPool
        {
            get => (_status & EntityStatus.IsFromPool) == EntityStatus.IsFromPool;
            set
            {
                if (value) _status |= EntityStatus.IsFromPool;
                else _status &= ~EntityStatus.IsFromPool;
            }
        }

        protected bool IsRegister
        {
            get => (_status & EntityStatus.IsRegister) == EntityStatus.IsRegister;
            set
            {
                if (IsRegister == value) return;

                if (value) _status |= EntityStatus.IsRegister;
                else _status &= ~EntityStatus.IsRegister;

                if (value) RegisterSystem();

                if (value)
                {
                    ViewGO = new GameObject(ViewName);
                    ViewGO.AddComponent<EntityComponentView>().Component = this;
                    ViewGO.transform.SetParent(_parent == null
                        ? EntityModule.Instance.EntityRoot.transform
                        : _parent.ViewGO.transform);
                }
                else
                {
                    UnityEngine.Object.Destroy(ViewGO);
                }
            }
        }

        internal bool IsComponent
        {
            get => (_status & EntityStatus.IsComponent) == EntityStatus.IsComponent;
            set
            {
                if (value) _status |= EntityStatus.IsComponent;
                else _status &= ~EntityStatus.IsComponent;
            }
        }

        protected bool IsCreated
        {
            get => (_status & EntityStatus.IsCreated) == EntityStatus.IsCreated;
            set
            {
                if (value) _status |= EntityStatus.IsCreated;
                else _status &= ~EntityStatus.IsCreated;
            }
        }

        protected bool IsNew
        {
            get => (_status & EntityStatus.IsNew) == EntityStatus.IsNew;
            set
            {
                if (value) _status |= EntityStatus.IsNew;
                else _status &= ~EntityStatus.IsNew;
            }
        }

        private string _viewName;
        protected virtual string ViewName
        {
            get
            {
                if (string.IsNullOrEmpty(_viewName)) _viewName = GetType().FullName;
                return _viewName;
            }
            set
            {
                _viewName = value;
                ViewGO.name = _viewName;
            }
        }

        public bool IsDisposed => InstanceId == 0;

        public Entity Parent
        {
            get => _parent;
            set
            {
                if (value == null) throw new Exception($"cant set parent null: {GetType().FullName}");
                if (value == this) throw new Exception($"cant set parent self: {GetType().FullName}");

                if (_parent != null)
                {
                    if (_parent == value)
                    {
                        Log.Error($"Duplicate parent set: {GetType().FullName} parent: {_parent.GetType().FullName}");
                        return;
                    }
                    _parent.RemoveFromChildren(this);
                }

                _parent = value;
                IsComponent = false;
                _parent.AddToChildren(this);

                if (this is IEntityScene scene) _iScene = scene;
                else IScene = _parent._iScene;

                ViewGO.GetComponent<EntityComponentView>().Component = this;
                ViewGO.transform.SetParent(_parent == null
                    ? EntityModule.Instance.EntityRoot.transform
                    : _parent.ViewGO.transform);

                if (_children != null)
                    foreach (var child in _children.Values)
                        child.ViewGO.transform.SetParent(ViewGO.transform);

                if (_components != null)
                    foreach (var comp in _components.Values)
                        comp.ViewGO.transform.SetParent(ViewGO.transform);
            }
        }

        private Entity ComponentParent
        {
            set
            {
                if (value == null) throw new Exception($"cant set parent null: {GetType().FullName}");
                if (value == this) throw new Exception($"cant set parent self: {GetType().FullName}");
                if (value.IScene == null) throw new Exception($"cant set parent because parent domain is null: {GetType().FullName} {value.GetType().FullName}");

                if (_parent != null)
                {
                    if (_parent == value)
                    {
                        Log.Error($"Duplicate parent set: {GetType().FullName} parent: {_parent.GetType().FullName}");
                        return;
                    }
                    _parent.RemoveFromComponents(this);
                }

                _parent = value;
                IsComponent = true;
                _parent.AddToComponents(this);

                if (this is IEntityScene scene) IScene = scene;
                else IScene = _parent._iScene;
            }
        }

        public IEntityScene IScene
        {
            get => _iScene;
            protected set
            {
                if (value == null) throw new Exception($"domain cant set null: {GetType().FullName}");
                if (_iScene == value) return;

                IEntityScene preScene = _iScene;
                _iScene = value;

                if (preScene == null)
                {
                    if (InstanceId == 0)
                        InstanceId = EntityIdGenerator.Instance.GenerateId();
                    IsRegister = true;
                }

                if (_children != null)
                    foreach (Entity entity in _children.Values)
                        entity.IScene = _iScene;

                if (_components != null)
                    foreach (Entity component in _components.Values)
                        component.IScene = _iScene;

                if (!IsCreated) IsCreated = true;
            }
        }

        public SortedDictionary<long, Entity> Children =>
            _children ??= new SortedDictionary<long, Entity>();

        public SortedDictionary<long, Entity> Components =>
            _components ??= new SortedDictionary<long, Entity>();

        protected Entity() { }

        protected virtual void RegisterSystem() { }

        public int ComponentsCount() => _components?.Count ?? 0;
        public int ChildrenCount() => _children?.Count ?? 0;

        public virtual void Dispose()
        {
            if (IsDisposed) return;

            IsRegister = false;
            InstanceId = 0;

            if (_children != null)
            {
                foreach (Entity child in _children.Values) child.Dispose();
                _children.Clear();
                _children = null;
            }

            if (_components != null)
            {
                foreach (var kv in _components) kv.Value.Dispose();
                _components.Clear();
                _components = null;
            }

            ComponentLifecycleManager.Instance?.UnregisterComponent(this);

            if (this is IEntityDestroy destroyable)
            {
                try { destroyable.OnDestroy(); }
                catch (Exception e) { Log.Error($"Destroy error: {e}"); }
            }

            _iScene = null;

            if (_parent != null && !_parent.IsDisposed)
            {
                if (IsComponent) _parent.RemoveComponent(this);
                else _parent.RemoveFromChildren(this);
            }

            _parent = null;

            bool isFromPool = IsFromPool;
            _status = EntityStatus.None;
            IsFromPool = isFromPool;
        }

        private void AddToChildren(Entity entity) => Children.Add(entity.Id, entity);

        private void RemoveFromChildren(Entity entity)
        {
            if (_children == null) return;
            _children.Remove(entity.Id);
            if (_children.Count == 0) _children = null;
        }

        private void AddToComponents(Entity component) =>
            Components.Add(GetLongHashCode(component.GetType()), component);

        private void RemoveFromComponents(Entity component)
        {
            if (_components == null) return;
            _components.Remove(GetLongHashCode(component.GetType()));
            if (_components.Count == 0) _components = null;
        }

        public K GetChild<K>(long id) where K : Entity
        {
            if (_children == null) return null;
            _children.TryGetValue(id, out Entity child);
            return child as K;
        }

        public void ClearChildren()
        {
            if (_children != null)
            {
                var childrenCopy = new System.Collections.Generic.List<Entity>(_children.Values);
                foreach (Entity child in childrenCopy) child.Dispose();
            }
        }

        public void RemoveChild(long id)
        {
            if (_children == null) return;
            if (!_children.TryGetValue(id, out Entity child)) return;
            _children.Remove(id);
            child.Dispose();
        }

        public void RemoveComponent<K>() where K : Entity
        {
            if (IsDisposed) return;
            if (_components == null) return;

            Type type = typeof(K);
            if (!_components.TryGetValue(GetLongHashCode(type), out Entity c)) return;

            EntityDependencyRegistry.Instance?.NotifyRemoveComponent(this, type);

            var registry = EntityDependencyRegistry.Instance;
            if (registry != null && (c is IEntityDependentComponent ||
                c.GetType().GetCustomAttributes(typeof(EntityDependsOnAttribute), true).Length > 0))
            {
                registry.UnregisterDependentComponent(c);
            }

            RemoveFromComponents(c);
            c.Dispose();
        }

        private void RemoveComponent(Entity component)
        {
            if (IsDisposed) return;
            if (_components == null) return;
            if (!_components.TryGetValue(GetLongHashCode(component.GetType()), out Entity c)) return;
            if (c.InstanceId != component.InstanceId) return;
            RemoveFromComponents(c);
            c.Dispose();
        }

        public void RemoveComponent(Type type)
        {
            if (IsDisposed) return;
            if (!_components.TryGetValue(GetLongHashCode(type), out Entity c)) return;
            RemoveFromComponents(c);
            c.Dispose();
        }

        public K GetComponent<K>() where K : Entity
        {
            if (_components == null) return null;
            if (_components.TryGetValue(GetLongHashCode(typeof(K)), out var exactMatch))
                return (K)exactMatch;
            foreach (var component in _components.Values)
                if (component is K derivedMatch)
                    return derivedMatch;
            return null;
        }

        public Entity GetComponent(Type type)
        {
            if (_components == null) return null;
            _components.TryGetValue(GetLongHashCode(type), out Entity component);
            return component;
        }

        internal static Entity Create(Type type, bool isFromPool)
        {
            Entity component = Activator.CreateInstance(type) as Entity;
            component.IsFromPool = isFromPool;
            component.IsCreated = true;
            component.IsNew = true;
            component.Id = 0;
            return component;
        }

        public Entity AddComponent(Entity component)
        {
            Type type = component.GetType();
            if (_components != null && _components.ContainsKey(GetLongHashCode(type)))
                throw new Exception($"entity already has component: {type.FullName}");
            component.ComponentParent = this;
            return component;
        }

        internal Entity AddComponent(Type type, bool isFromPool = false)
        {
            if (_components != null && _components.ContainsKey(GetLongHashCode(type)))
                throw new Exception($"entity already has component: {type.FullName}");
            Entity component = Create(type, isFromPool);
            component.Id = Id;
            component.ComponentParent = this;
            return component;
        }

        public K AddComponentWithId<K>(long id, bool isFromPool = false) where K : Entity, IEntityAwake, new()
        {
            Type type = typeof(K);
            if (_components != null && _components.ContainsKey(GetLongHashCode(type)))
                throw new Exception($"entity already has component: {type.FullName}");
            Entity component = Create(type, isFromPool);
            component.Id = id;
            component.ComponentParent = this;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component);
            lifecycle.RegisterComponent(component);
            return component as K;
        }

        public K AddComponentWithId<K, P1>(long id, P1 p1, bool isFromPool = false) where K : Entity, IEntityAwake<P1>, new()
        {
            Type type = typeof(K);
            if (_components != null && _components.ContainsKey(GetLongHashCode(type)))
                throw new Exception($"entity already has component: {type.FullName}");
            Entity component = Create(type, isFromPool);
            component.Id = id;
            component.ComponentParent = this;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, p1);
            lifecycle.RegisterComponent(component);
            return component as K;
        }

        public K AddComponentWithId<K, P1, P2>(long id, P1 p1, P2 p2, bool isFromPool = false) where K : Entity, IEntityAwake<P1, P2>, new()
        {
            Type type = typeof(K);
            if (_components != null && _components.ContainsKey(GetLongHashCode(type)))
                throw new Exception($"entity already has component: {type.FullName}");
            Entity component = Create(type, isFromPool);
            component.Id = id;
            component.ComponentParent = this;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, p1, p2);
            lifecycle.RegisterComponent(component);
            return component as K;
        }

        public K AddComponentWithId<K, P1, P2, P3>(long id, P1 p1, P2 p2, P3 p3, bool isFromPool = false) where K : Entity, IEntityAwake<P1, P2, P3>, new()
        {
            Type type = typeof(K);
            if (_components != null && _components.ContainsKey(GetLongHashCode(type)))
                throw new Exception($"entity already has component: {type.FullName}");
            Entity component = Create(type, isFromPool);
            component.Id = id;
            component.ComponentParent = this;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, p1, p2, p3);
            lifecycle.RegisterComponent(component);
            return component as K;
        }

        public K AddComponent<K>(bool isFromPool = false) where K : Entity, IEntityAwake, new()
        {
            K component = AddComponentWithId<K>(Id, isFromPool);
            EntityDependencyRegistry.Instance?.NotifyAddComponent(this, typeof(K));
            component.ProcessComponentDependencies();
            return component;
        }

        public K AddComponent<K, P1>(P1 p1, bool isFromPool = false) where K : Entity, IEntityAwake<P1>, new()
        {
            K component = AddComponentWithId<K, P1>(Id, p1, isFromPool);
            EntityDependencyRegistry.Instance?.NotifyAddComponent(this, typeof(K));
            component.ProcessComponentDependencies();
            return component;
        }

        public K AddComponent<K, P1, P2>(P1 p1, P2 p2, bool isFromPool = false) where K : Entity, IEntityAwake<P1, P2>, new()
        {
            K component = AddComponentWithId<K, P1, P2>(Id, p1, p2, isFromPool);
            EntityDependencyRegistry.Instance?.NotifyAddComponent(this, typeof(K));
            component.ProcessComponentDependencies();
            return component;
        }

        public K AddComponent<K, P1, P2, P3>(P1 p1, P2 p2, P3 p3, bool isFromPool = false) where K : Entity, IEntityAwake<P1, P2, P3>, new()
        {
            K component = AddComponentWithId<K, P1, P2, P3>(Id, p1, p2, p3, isFromPool);
            EntityDependencyRegistry.Instance?.NotifyAddComponent(this, typeof(K));
            component.ProcessComponentDependencies();
            return component;
        }

        internal Entity AddChild(Entity entity)
        {
            entity.Parent = this;
            return entity;
        }

        public T AddChild<T>(bool isFromPool = false) where T : Entity, IEntityAwake
        {
            T component = (T)Entity.Create(typeof(T), isFromPool);
            component.Id = EntityIdGenerator.Instance.GenerateId();
            component.Parent = this;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component);
            lifecycle.RegisterComponent(component);
            return component;
        }

        public T AddChild<T, A>(A a, bool isFromPool = false) where T : Entity, IEntityAwake<A>
        {
            T component = (T)Entity.Create(typeof(T), isFromPool);
            component.Id = EntityIdGenerator.Instance.GenerateId();
            component.Parent = this;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, a);
            lifecycle.RegisterComponent(component);
            return component;
        }

        public T AddChild<T, A, B>(A a, B b, bool isFromPool = false) where T : Entity, IEntityAwake<A, B>
        {
            T component = (T)Entity.Create(typeof(T), isFromPool);
            component.Id = EntityIdGenerator.Instance.GenerateId();
            component.Parent = this;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, a, b);
            lifecycle.RegisterComponent(component);
            return component;
        }

        public T AddChild<T, A, B, C>(A a, B b, C c, bool isFromPool = false) where T : Entity, IEntityAwake<A, B, C>
        {
            T component = (T)Entity.Create(typeof(T), isFromPool);
            component.Id = EntityIdGenerator.Instance.GenerateId();
            component.Parent = this;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, a, b, c);
            lifecycle.RegisterComponent(component);
            return component;
        }

        public T AddChildWithId<T>(long id, bool isFromPool = false) where T : Entity, IEntityAwake
        {
            T component = (T)Entity.Create(typeof(T), isFromPool);
            component.Id = id;
            component.Parent = this;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component);
            lifecycle.RegisterComponent(component);
            return component;
        }

        public T AddChildWithId<T, A>(long id, A a, bool isFromPool = false) where T : Entity, IEntityAwake<A>
        {
            T component = (T)Entity.Create(typeof(T), isFromPool);
            component.Id = id;
            component.Parent = this;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, a);
            lifecycle.RegisterComponent(component);
            return component;
        }

        public T AddChildWithId<T, A, B>(long id, A a, B b, bool isFromPool = false) where T : Entity, IEntityAwake<A, B>
        {
            T component = (T)Entity.Create(typeof(T), isFromPool);
            component.Id = id;
            component.Parent = this;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, a, b);
            lifecycle.RegisterComponent(component);
            return component;
        }

        public T AddChildWithId<T, A, B, C>(long id, A a, B b, C c, bool isFromPool = false) where T : Entity, IEntityAwake<A, B, C>
        {
            T component = (T)Entity.Create(typeof(T), isFromPool);
            component.Id = id;
            component.Parent = this;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, a, b, c);
            lifecycle.RegisterComponent(component);
            return component;
        }

        internal protected virtual long GetLongHashCode(Type type) => type.TypeHandle.Value.ToInt64();
    }
}
