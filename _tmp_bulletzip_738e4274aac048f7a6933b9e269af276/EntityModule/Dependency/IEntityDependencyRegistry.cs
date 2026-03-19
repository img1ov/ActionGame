using System;

namespace GameLogic
{
    public interface IEntityDependencyRegistry
    {
        void RegisterDependentComponent(Entity component, Type[] dependencies);
        void UnregisterDependentComponent(Entity component);
        void NotifyComponentChanged(Entity entity, Type componentType, bool isAdded);
    }
}
