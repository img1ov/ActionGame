using System;

namespace GameLogic
{
    [AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
    public class EntityDependsOnAttribute : Attribute
    {
        public Type[] DependencyTypes { get; }
        public EntityDependsOnAttribute(params Type[] dependencyTypes)
        {
            DependencyTypes = dependencyTypes ?? new Type[0];
        }
    }
}
