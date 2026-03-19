using System;
using System.Linq;

namespace GameLogic
{
    public abstract class EntityDependentComponentBase : Entity, IEntityDependentComponent
    {
        public bool AreAllDependenciesMet { get; private set; }

        public virtual Type[] GetDependencyTypes()
        {
            return GetType().GetCustomAttributes(typeof(EntityDependsOnAttribute), true)
                .Cast<EntityDependsOnAttribute>()
                .SelectMany(attr => attr.DependencyTypes)
                .Distinct()
                .ToArray();
        }

        public void OnDependencyStatusChanged(bool areAllDependenciesMet)
        {
            if (AreAllDependenciesMet == areAllDependenciesMet) return;
            AreAllDependenciesMet = areAllDependenciesMet;
            OnActivationChanged(areAllDependenciesMet);
        }

        protected virtual void OnActivationChanged(bool isActive) { }
    }
}
