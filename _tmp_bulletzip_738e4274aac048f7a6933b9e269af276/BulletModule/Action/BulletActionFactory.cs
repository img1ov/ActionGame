namespace GameLogic
{
    internal sealed class BulletActionFactory
    {
        public BulletActionBase Create(BulletActionType actionType)
        {
            return actionType switch
            {
                BulletActionType.Init => new BulletInitAction(),
                BulletActionType.Lifetime => new BulletLifetimeAction(),
                BulletActionType.Destroy => new BulletDestroyAction(),
                BulletActionType.DelayDestroy => new BulletDelayDestroyAction(),
                BulletActionType.Callback => new BulletCallbackAction(),
                _ => null,
            };
        }
    }
}
