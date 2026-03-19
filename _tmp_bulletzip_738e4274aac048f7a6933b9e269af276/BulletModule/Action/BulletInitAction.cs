namespace GameLogic
{
    internal sealed class BulletInitAction : BulletActionBase
    {
        protected override void OnExecute()
        {
            BulletRuntimeData runtime = Bullet?.Runtime;
            if (runtime == null)
            {
                IsFinished = true;
                return;
            }

            runtime.IsInitialized = true;
            Module.AddAction(Bullet, BulletActionInfo.Create(BulletActionType.Lifetime));
            IsFinished = true;
        }
    }
}
