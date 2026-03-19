namespace GameLogic
{
    // Entity生命周期接口 - 使用IEntity前缀避免与SingletonSystem的接口冲突

    public interface IEntityAwake
    {
        void Awake();
    }

    public interface IEntityAwake<T>
    {
        void Awake(T p1);
    }

    public interface IEntityAwake<T1, T2>
    {
        void Awake(T1 p1, T2 p2);
    }

    public interface IEntityAwake<T1, T2, T3>
    {
        void Awake(T1 p1, T2 p2, T3 p3);
    }

    public interface IEntityAwake<T1, T2, T3, T4>
    {
        void Awake(T1 p1, T2 p2, T3 p3, T4 p4);
    }

    public interface IEntityUpdate
    {
        void Update(float deltaTime);
    }

    public interface IEntityFixedUpdate
    {
        void FixedUpdate(float fixedDeltaTime);
    }

    public interface IEntityLateUpdate
    {
        void LateUpdate();
    }

    public interface IEntityDestroy
    {
        void OnDestroy();
    }
}
