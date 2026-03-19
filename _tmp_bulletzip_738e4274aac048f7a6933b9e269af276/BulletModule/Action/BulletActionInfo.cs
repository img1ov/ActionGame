using System;

namespace GameLogic
{
    public sealed class BulletActionInfo
    {
        public BulletActionType ActionType;
        public float DelaySeconds;
        public bool IgnoreBulletTimeScale;
        public BulletDestroyReason DestroyReason;
        public Action<BulletEntity, BulletModule> Callback;

        public static BulletActionInfo Create(BulletActionType actionType)
        {
            return new BulletActionInfo { ActionType = actionType };
        }
    }
}
