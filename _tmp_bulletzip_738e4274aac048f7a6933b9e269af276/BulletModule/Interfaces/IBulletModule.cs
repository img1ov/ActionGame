namespace GameLogic
{
    public interface IBulletModule
    {
        BulletEntity Spawn(BulletSpawnParams spawnParams);
        bool Destroy(long bulletId, BulletDestroyReason reason = BulletDestroyReason.Manual);
        void DestroyByOwner(long ownerId, BulletDestroyReason reason = BulletDestroyReason.Manual);
        bool Freeze(long bulletId, float durationSeconds = 0f);
        void FreezeByOwner(long ownerId, float durationSeconds = 0f);
        bool Unfreeze(long bulletId);
        void UnfreezeByOwner(long ownerId);
        bool SetBulletTimeScale(long bulletId, float timeScale);
        void SetBulletTimeScaleByOwner(long ownerId, float timeScale);
        bool SetInheritedTimeScale(long bulletId, float timeScale);
        void SetInheritedTimeScaleByOwner(long ownerId, float timeScale);
        BulletEntity Get(long bulletId);
        bool TryGet(long bulletId, out BulletEntity bullet);
        bool RegisterConfig(BulletConfig config);
        bool TryGetConfig(string configId, out BulletConfig config);
        void Clear();
    }
}
