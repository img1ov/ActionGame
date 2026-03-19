namespace GameLogic
{
    public abstract class BulletActionBase
    {
        protected BulletEntity Bullet { get; private set; }
        protected BulletModule Module { get; private set; }
        protected BulletActionInfo ActionInfo { get; private set; }

        public bool IsFinished { get; protected set; }
        public bool IsPersistent { get; protected set; }

        public void Execute(BulletEntity bullet, BulletModule module, BulletActionInfo actionInfo)
        {
            Bullet = bullet;
            Module = module;
            ActionInfo = actionInfo;
            IsFinished = false;
            OnExecute();
        }

        public virtual void Tick(float deltaTime)
        {
        }

        public virtual void AfterTick(float deltaTime)
        {
        }

        public virtual void Reset()
        {
            Bullet = null;
            Module = null;
            ActionInfo = null;
            IsFinished = false;
            IsPersistent = false;
        }

        protected abstract void OnExecute();
    }
}
