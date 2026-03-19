using TEngine;

namespace GameLogic
{
    [ChildOf]
    public abstract class EntityScene : Entity, IEntityScene, IEntityAwake, IEntityDestroy
    {
        public string Name { get; }

        public EntityScene(string name)
        {
            this.Name = name;
            this.Id = EntityIdGenerator.Instance.GenerateId();
            this.InstanceId = EntityIdGenerator.Instance.GenerateInstanceId();
            this.IsCreated = true;
            this.IsNew = true;
            this.IScene = this;
            this.IsRegister = true;
        }

        public EntityScene(long id, long instanceId, string name)
        {
            this.Id = id;
            this.Name = name;
            this.InstanceId = instanceId;
            this.IsCreated = true;
            this.IsNew = true;
            this.IScene = this;
            this.IsRegister = true;
        }

        public override void Dispose()
        {
            base.Dispose();
            Log.Info($"scene dispose: {this.Name} {this.Id} {this.InstanceId}");
        }

        public virtual void Awake()
        {
            Log.Info($"scene awake: {this.Name} {this.Id} {this.InstanceId}");
        }

        public virtual void OnDestroy()
        {
            Log.Info($"scene destroy: {this.Name} {this.Id} {this.InstanceId}");
        }

        protected override string ViewName => $"{this.GetType().Name}";
    }
}
