using System;

namespace GameLogic
{
    public interface IComponentManager
    {
        Type ComponentType { get; }
        int Count { get; }
        void Register(Entity component);
        void Unregister(Entity component);
    }

    public interface IUpdatableManager
    {
        void UpdateAll(float deltaTime);
    }

    public interface IFixedUpdatableManager
    {
        void FixedUpdateAll(float fixedDeltaTime);
    }

    public interface ILateUpdatableManager
    {
        void LateUpdateAll();
    }
}
