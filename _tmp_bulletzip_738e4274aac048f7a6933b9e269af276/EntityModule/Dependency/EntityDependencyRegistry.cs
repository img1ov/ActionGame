using System;
using System.Collections.Generic;
using System.Linq;

namespace GameLogic
{
    public class EntityDependencyRegistry : Singleton<EntityDependencyRegistry>, IEntityDependencyRegistry
    {
        private Dictionary<Type, HashSet<Entity>> _dependencyDict = new();
        private Dictionary<Entity, Type[]> _componentDependencies = new();

        public delegate void ComponentChangeHandler(Entity entity, Type componentType);
        public event ComponentChangeHandler OnComponentAdded;
        public event ComponentChangeHandler OnComponentRemoved;

        protected override void OnInit()
        {
            OnComponentAdded += (entity, type) => NotifyComponentChanged(entity, type, true);
            OnComponentRemoved += (entity, type) => NotifyComponentChanged(entity, type, false);
        }

        public void RegisterDependentComponent(Entity component, Type[] dependencies)
        {
            if (component == null || dependencies == null || dependencies.Length == 0) return;
            _componentDependencies[component] = dependencies;
            foreach (var depType in dependencies)
            {
                if (!_dependencyDict.TryGetValue(depType, out var comps))
                {
                    comps = new HashSet<Entity>();
                    _dependencyDict[depType] = comps;
                }
                comps.Add(component);
            }
            CheckDependenciesForComponent(component);
        }

        public void UnregisterDependentComponent(Entity component)
        {
            if (component == null || !_componentDependencies.TryGetValue(component, out var deps)) return;
            foreach (var depType in deps)
            {
                if (_dependencyDict.TryGetValue(depType, out var comps))
                {
                    comps.Remove(component);
                    if (comps.Count == 0) _dependencyDict.Remove(depType);
                }
            }
            _componentDependencies.Remove(component);
        }

        public void NotifyComponentChanged(Entity entity, Type componentType, bool isAdded)
        {
            if (entity == null || componentType == null) return;
            if (_dependencyDict.TryGetValue(componentType, out var dependentComponents))
            {
                var components = dependentComponents.ToArray();
                foreach (var component in components)
                {
                    if (component.Parent == entity) CheckDependenciesForComponent(component);
                }
            }
        }

        private void CheckDependenciesForComponent(Entity component)
        {
            if (!_componentDependencies.TryGetValue(component, out var dependencies)) return;
            var parent = component.Parent;
            if (parent == null) return;

            bool allMet = true;
            foreach (var depType in dependencies)
            {
                if (parent.GetComponent(depType) == null) { allMet = false; break; }
            }
            if (component is IEntityDependentComponent dep) dep.OnDependencyStatusChanged(allMet);
        }

        public void NotifyAddComponent(Entity entity, Type componentType) =>
            OnComponentAdded?.Invoke(entity, componentType);

        public void NotifyRemoveComponent(Entity entity, Type componentType) =>
            OnComponentRemoved?.Invoke(entity, componentType);

        protected override void OnRelease()
        {
            _dependencyDict.Clear();
            _componentDependencies.Clear();
        }
    }
}
