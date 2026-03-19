namespace GameLogic
{
    internal sealed class BulletLifetimeAction : BulletActionBase
    {
        protected override void OnExecute()
        {
            IsPersistent = true;
        }

        public override void AfterTick(float deltaTime)
        {
            BulletRuntimeData runtime = Bullet?.Runtime;
            if (runtime == null || Bullet.IsDisposed || runtime.DestroyRequested || runtime.PendingDestroy)
            {
                IsFinished = true;
                return;
            }

            float tickDeltaTime = runtime.GetActionDeltaTime(false);
            if (tickDeltaTime <= 0f)
            {
                return;
            }

            runtime.Lifetime += tickDeltaTime;
            if (runtime.MaxLifetime <= 0f || runtime.Lifetime < runtime.MaxLifetime)
            {
                return;
            }

            runtime.Lifetime = runtime.MaxLifetime;
            Module.RequestDestroy(Bullet, BulletDestroyReason.LifetimeExpired);
            IsFinished = true;
        }
    }
}
