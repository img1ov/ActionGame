using System;
using System.Collections.Generic;
using TEngine;

namespace GameLogic
{
    public class ComponentLifecycleManager : Singleton<ComponentLifecycleManager>
    {
        private readonly Dictionary<Type, IComponentManager> _updateManagers = new();
        private readonly Dictionary<Type, IComponentManager> _fixedUpdateManagers = new();
        private readonly Dictionary<Type, IComponentManager> _lateUpdateManagers = new();
        private readonly List<IUpdatableManager> _updatableList = new();
        private readonly List<IFixedUpdatableManager> _fixedUpdatableList = new();
        private readonly List<ILateUpdatableManager> _lateUpdatableList = new();

        public void RegisterComponent(Entity component)
        {
            Type type = component.GetType();

            if (component is IEntityUpdate)
            {
                if (!_updateManagers.TryGetValue(type, out var mgr))
                {
                    var mgrType = typeof(ComponentManager<>).MakeGenericType(type);
                    mgr = (IComponentManager)Activator.CreateInstance(mgrType);
                    _updateManagers[type] = mgr;
                    if (mgr is IUpdatableManager u) _updatableList.Add(u);
                }
                mgr.Register(component);
            }

            if (component is IEntityFixedUpdate)
            {
                if (!_fixedUpdateManagers.TryGetValue(type, out var mgr))
                {
                    var mgrType = typeof(ComponentManager<>).MakeGenericType(type);
                    mgr = (IComponentManager)Activator.CreateInstance(mgrType);
                    _fixedUpdateManagers[type] = mgr;
                    if (mgr is IFixedUpdatableManager f) _fixedUpdatableList.Add(f);
                }
                mgr.Register(component);
            }

            if (component is IEntityLateUpdate)
            {
                if (!_lateUpdateManagers.TryGetValue(type, out var mgr))
                {
                    var mgrType = typeof(ComponentManager<>).MakeGenericType(type);
                    mgr = (IComponentManager)Activator.CreateInstance(mgrType);
                    _lateUpdateManagers[type] = mgr;
                    if (mgr is ILateUpdatableManager l) _lateUpdatableList.Add(l);
                }
                mgr.Register(component);
            }
        }

        public void UnregisterComponent(Entity component)
        {
            Type type = component.GetType();
            if (_updateManagers.TryGetValue(type, out var uMgr)) uMgr.Unregister(component);
            if (_fixedUpdateManagers.TryGetValue(type, out var fMgr)) fMgr.Unregister(component);
            if (_lateUpdateManagers.TryGetValue(type, out var lMgr)) lMgr.Unregister(component);
        }

        public void UpdateAll(float deltaTime)
        {
            for (int i = 0; i < _updatableList.Count; i++)
                _updatableList[i].UpdateAll(deltaTime);
        }

        public void FixedUpdateAll(float fixedDeltaTime)
        {
            for (int i = 0; i < _fixedUpdatableList.Count; i++)
                _fixedUpdatableList[i].FixedUpdateAll(fixedDeltaTime);
        }

        public void LateUpdateAll()
        {
            for (int i = 0; i < _lateUpdatableList.Count; i++)
                _lateUpdatableList[i].LateUpdateAll();
        }

        public void AwakeEntity(Entity entity)
        {
            try { if (entity is IEntityAwake a) a.Awake(); }
            catch (Exception e) { Log.Error($"Awake error: {e}"); }
        }

        public void AwakeEntity<P1>(Entity entity, P1 p1)
        {
            try { if (entity is IEntityAwake<P1> a) a.Awake(p1); }
            catch (Exception e) { Log.Error($"Awake error: {e}"); }
        }

        public void AwakeEntity<P1, P2>(Entity entity, P1 p1, P2 p2)
        {
            try { if (entity is IEntityAwake<P1, P2> a) a.Awake(p1, p2); }
            catch (Exception e) { Log.Error($"Awake error: {e}"); }
        }

        public void AwakeEntity<P1, P2, P3>(Entity entity, P1 p1, P2 p2, P3 p3)
        {
            try { if (entity is IEntityAwake<P1, P2, P3> a) a.Awake(p1, p2, p3); }
            catch (Exception e) { Log.Error($"Awake error: {e}"); }
        }

        public void AwakeEntity<P1, P2, P3, P4>(Entity entity, P1 p1, P2 p2, P3 p3, P4 p4)
        {
            try { if (entity is IEntityAwake<P1, P2, P3, P4> a) a.Awake(p1, p2, p3, p4); }
            catch (Exception e) { Log.Error($"Awake error: {e}"); }
        }

        protected override void OnRelease()
        {
            _updateManagers.Clear();
            _fixedUpdateManagers.Clear();
            _lateUpdateManagers.Clear();
            _updatableList.Clear();
            _fixedUpdatableList.Clear();
            _lateUpdatableList.Clear();
        }
    }
}
