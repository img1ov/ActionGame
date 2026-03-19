using System;

namespace GameLogic
{
    [AttributeUsage(AttributeTargets.Class)]
    public class ChildOfAttribute : Attribute
    {
        public Type Type { get; private set; }
        public ChildOfAttribute(Type type = null) { Type = type; }
    }

    [AttributeUsage(AttributeTargets.Class)]
    public class ComponentOfAttribute : Attribute
    {
        public Type ComponentType { get; private set; }
        public ComponentOfAttribute(Type componentType) { ComponentType = componentType; }
    }
}
