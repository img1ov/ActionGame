using System;

namespace GameLogic
{
    public interface IEntityDependentComponent
    {
        Type[] GetDependencyTypes();
        void OnDependencyStatusChanged(bool areAllDependenciesMet);
        bool AreAllDependenciesMet { get; }
    }
}
