using System;
using System.Collections.Generic;
using TEngine;

namespace GameLogic
{
    public class ComponentManager<T> : IComponentManager, IUpdatableManager, IFixedUpdatableManager, ILateUpdatableManager where T : Entity
    {
        private readonly List<T> _components;
        private readonly HashSet<T> _pendingRemove = new HashSet<T>();
        private bool _isIterating;

        public Type ComponentType => typeof(T);
        public int Count => _components.Count;

        public ComponentManager(int initialCapacity = 16)
        {
            _components = new List<T>(initialCapacity);
        }

        public void Register(Entity component)
        {
            if (component is T typed) _components.Add(typed);
        }

        public void Unregister(Entity component)
        {
            if (component is T typed)
            {
                if (_isIterating) _pendingRemove.Add(typed);
                else _components.Remove(typed);
            }
        }

        public void UpdateAll(float deltaTime)
        {
            _isIterating = true;
            int count = _components.Count;
            for (int i = 0; i < count; i++)
            {
                var comp = _components[i];
                if (comp.IsDisposed || _pendingRemove.Contains(comp)) continue;
                if (comp is IEntityDependentComponent dep && !dep.AreAllDependenciesMet) continue;
                if (comp is IEntityUpdate updatable)
                {
                    try { updatable.Update(deltaTime); }
                    catch (Exception e) { Log.Error($"ComponentManager Update error [{typeof(T).Name}]: {e}"); }
                }
            }
            _isIterating = false;
            FlushPendingRemove();
        }

        public void FixedUpdateAll(float fixedDeltaTime)
        {
            _isIterating = true;
            int count = _components.Count;
            for (int i = 0; i < count; i++)
            {
                var comp = _components[i];
                if (comp.IsDisposed || _pendingRemove.Contains(comp)) continue;
                if (comp is IEntityDependentComponent dep && !dep.AreAllDependenciesMet) continue;
                if (comp is IEntityFixedUpdate fixedUpdatable)
                {
                    try { fixedUpdatable.FixedUpdate(fixedDeltaTime); }
                    catch (Exception e) { Log.Error($"ComponentManager FixedUpdate error [{typeof(T).Name}]: {e}"); }
                }
            }
            _isIterating = false;
            FlushPendingRemove();
        }

        public void LateUpdateAll()
        {
            _isIterating = true;
            int count = _components.Count;
            for (int i = 0; i < count; i++)
            {
                var comp = _components[i];
                if (comp.IsDisposed || _pendingRemove.Contains(comp)) continue;
                if (comp is IEntityDependentComponent dep && !dep.AreAllDependenciesMet) continue;
                if (comp is IEntityLateUpdate lateUpdatable)
                {
                    try { lateUpdatable.LateUpdate(); }
                    catch (Exception e) { Log.Error($"ComponentManager LateUpdate error [{typeof(T).Name}]: {e}"); }
                }
            }
            _isIterating = false;
            FlushPendingRemove();
        }

        private void FlushPendingRemove()
        {
            if (_pendingRemove.Count > 0)
            {
                foreach (var comp in _pendingRemove) _components.Remove(comp);
                _pendingRemove.Clear();
            }
        }
    }
}
