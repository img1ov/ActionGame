namespace GameLogic
{
    internal sealed class BulletCallbackAction : BulletActionBase
    {
        protected override void OnExecute()
        {
            ActionInfo?.Callback?.Invoke(Bullet, Module);
            IsFinished = true;
        }
    }
}
