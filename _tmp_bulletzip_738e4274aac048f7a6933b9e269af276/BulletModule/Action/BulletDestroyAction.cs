namespace GameLogic
{
    internal sealed class BulletDestroyAction : BulletActionBase
    {
        protected override void OnExecute()
        {
            BulletRuntimeData runtime = Bullet?.Runtime;
            if (runtime == null || Bullet.IsDisposed)
            {
                IsFinished = true;
                return;
            }

            if (!runtime.PendingDestroy)
            {
                Module.LifecycleSystem.MarkDestroy(Bullet, ActionInfo?.DestroyReason ?? runtime.DestroyReason);
            }

            IsFinished = true;
        }
    }
}
