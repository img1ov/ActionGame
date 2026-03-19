using System;
using System.Linq;

namespace GameLogic
{
    public static class EntityDependencyExtensions
    {
        public static bool AreDependenciesMet<T>(this Entity entity) where T : Entity
        {
            var component = entity.GetComponent<T>();
            if (component == null) return false;
            return component is IEntityDependentComponent dep ? dep.AreAllDependenciesMet : true;
        }

        internal static void ProcessComponentDependencies(this Entity component)
        {
            if (component == null) return;
            var registry = EntityDependencyRegistry.Instance;
            if (registry == null) return;

            if (component is IEntityDependentComponent dep)
            {
                var types = dep.GetDependencyTypes();
                if (types != null && types.Length > 0) registry.RegisterDependentComponent(component, types);
                return;
            }

            var attributes = component.GetType().GetCustomAttributes(typeof(EntityDependsOnAttribute), true);
            if (attributes.Length > 0)
            {
                var types = attributes.Cast<EntityDependsOnAttribute>()
                    .SelectMany(attr => attr.DependencyTypes)
                    .Distinct().ToArray();
                if (types.Length > 0) registry.RegisterDependentComponent(component, types);
            }
        }
    }
}
