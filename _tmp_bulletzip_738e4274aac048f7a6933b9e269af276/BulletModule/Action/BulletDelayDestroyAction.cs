namespace GameLogic
{
    internal sealed class BulletDelayDestroyAction : BulletActionBase
    {
        private float _remainingSeconds;

        protected override void OnExecute()
        {
            BulletRuntimeData runtime = Bullet?.Runtime;
            if (runtime == null || Bullet.IsDisposed || runtime.DestroyRequested || runtime.PendingDestroy)
            {
                IsFinished = true;
                return;
            }

            _remainingSeconds = ActionInfo != null ? ActionInfo.DelaySeconds : 0f;
            IsPersistent = _remainingSeconds > 0f;

            if (!IsPersistent)
            {
                Module.RequestDestroy(Bullet, ActionInfo?.DestroyReason ?? BulletDestroyReason.Manual);
                IsFinished = true;
            }
        }

        public override void Tick(float deltaTime)
        {
            BulletRuntimeData runtime = Bullet?.Runtime;
            if (runtime == null || Bullet.IsDisposed || runtime.DestroyRequested || runtime.PendingDestroy)
            {
                IsFinished = true;
                return;
            }

            float tickDeltaTime = runtime.GetActionDeltaTime(ActionInfo?.IgnoreBulletTimeScale ?? false);
            if (tickDeltaTime <= 0f)
            {
                return;
            }

            _remainingSeconds -= tickDeltaTime;
            if (_remainingSeconds > 0f)
            {
                return;
            }

            Module.RequestDestroy(Bullet, ActionInfo?.DestroyReason ?? BulletDestroyReason.Manual);
            IsFinished = true;
        }

        public override void Reset()
        {
            base.Reset();
            _remainingSeconds = 0f;
        }
    }
}
